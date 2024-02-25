// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/hunspell/google/bdict_writer.h"

#include <stddef.h>
#include <stdint.h>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/strings/stringprintf.h"
#include "third_party/hunspell/google/bdict.h"

namespace hunspell {

// Represents one node the word trie in memory. This does not have to be very
// efficient since it is only used when building.
class DicNode {
 public:
  enum StorageType {
    UNDEFINED,  // Uninitialized storage type.
    LEAF,       // Word with no additional string data.
    LEAFMORE,   // Word with additional suffix following.
    LIST16,     // List of sub-nodes with 16-bit relative offsets.
    LIST8,      // List of sub-nodes with 8-bit relative offsets.
    LOOKUP32,   // Lookup table with 32-bit absolute offsets.
    LOOKUP16,   // LOokup table with 16-bit relative offsets.
  };

  DicNode() : addition(0), storage(UNDEFINED) {
  }

  ~DicNode() {
    for (size_t i = 0; i < children.size(); i++)
      delete children[i];
  }

  bool is_leaf() const { return children.empty(); }

  // When non-zero, this character is the additional level that this
  // node represents. This will be 0 for some leaf nodes when there is no
  // addition and for the root node.
  char addition;

  std::vector<DicNode*> children;

  // When there are no children, this is a leaf node and this "addition string"
  // is appended to the result. When there are children, this will be empty.
  std::string leaf_addition;

  // For leaf nodes, this are the indices into the affix table.
  std::vector<int> affix_indices;

  // Initially uninitialized, ComputeStorage() will fill this in with the
  // desired serialization method.
  StorageType storage;
};

namespace {

void SerializeTrie(const DicNode* node, std::string* output);

// Returns true if the nth character in the given word is |ch|. Will return
// false when there is no nth character. Note that this will also match an
// implicit NULL at the end of the string.
bool NthCharacterIs(const std::string& word, size_t n, char ch) {
  if (word.length() < n)  // Want to allow n == length() to catch the NULL.
    return false;
  return word.c_str()[n] == ch;  // Use c_str() to get NULL terminator.
}

// Recursively build the trie data structure for the range in the |words| list
// in [begin, end). It is assumed that all words in that range will have the
// same |node_depth - 2| characters at the beginning. This node will key off of
// the |node_depth - 1| character, with a special case for the root.
//
// |prefix_chars| is how deep this node is in the trie (and corresponds to how
// many letters of the word we will skip). The root level will have
// |prefix_chars| of 0.
//
// The given |node| will be filled with the data. The return value is the
// index into the |words| vector of the next word to process. It will be
// equal to |end| when all words have been consumed.
size_t BuildTrie(const BDictWriter::WordList& words,
                 size_t begin, size_t end,
                 size_t node_depth, DicNode* node) {
  // Find the prefix that this node represents.
  const std::string& begin_str = words[begin].first;
  if (begin_str.length() < node_depth) {
    // Singleton.
    node->addition = 0;
    node->affix_indices = words[begin].second;
    return begin + 1;
  }

  // Now find the range of words sharing this prefix.
  size_t match_count;
  if (node_depth == 0 && begin == 0) {
    // Special case the root node.
    match_count = end - begin;
    node->addition = 0;
  } else {
    match_count = 0;
    node->addition = begin_str[node_depth - 1];
    // We know the strings should have [0, node_depth-1) characters at the
    // beginning already matching, so we only need to check the new one.
    while (begin + match_count < end &&
           NthCharacterIs(words[begin + match_count].first,
                          node_depth - 1, node->addition))
      match_count++;
  }

  if (match_count == 1) {
    // Just found a leaf node with no other words sharing its prefix. Save any
    // remaining characters and we're done.
    node->affix_indices = words[begin].second;
    node->leaf_addition = begin_str.substr(node_depth);
    return begin + 1;
  }

  // We have a range of words, add them as children of this node.
  size_t i = begin;
  while (i < begin + match_count) {
    DicNode* cur = new DicNode;
    i = BuildTrie(words, i, begin + match_count, node_depth + 1, cur);
    node->children.push_back(cur);
  }

  return begin + match_count;
}

// Lookup tables are complicated. They can have a magic 0th entry not counted
// in the table dimensions, and also have indices only for the used sub-range.
// This function will compute the starting point and size of a lookup table,
// in addition to whether it should have the magic 0th entry for the given
// list of child nodes.
void ComputeLookupStrategyDetails(const std::vector<DicNode*>& children,
                                  bool* has_0th_entry,
                                  int* first_item,
                                  int* list_size) {
  *has_0th_entry = false;
  *first_item = 0;
  *list_size = 0;
  if (children.empty())
    return;

  size_t first_offset = 0;
  if (children[0]->addition == 0) {
    *has_0th_entry = true;
    first_offset++;
  }

  if (children.size() == first_offset)
    return;

  *first_item = static_cast<unsigned char>(children[first_offset]->addition);
  unsigned char last_item = children[children.size() - 1]->addition;
  *list_size = last_item - *first_item + 1;
}

// Recursively fills in the storage strategy for this node and each of its
// children. This must be done before actually serializing because the storage
// mode will depend on the size of the children.
size_t ComputeTrieStorage(DicNode* node) {
  if (node->is_leaf()) {
    // The additional affix list holds affixes when there is more than one. Each
    // entry is two bytes, plus an additional FFFF terminator.
    size_t supplimentary_size = 0;
    if (node->affix_indices[0] > BDict::LEAF_NODE_MAX_FIRST_AFFIX_ID) {
      // We cannot store the first affix ID of the affix list into a leaf node.
      // In this case, we have to store all the affix IDs and a terminator
      // into a supplimentary list.
      supplimentary_size = node->affix_indices.size() * 2 + 2;
    } else if (node->affix_indices.size() > 1) {
      // We can store the first affix ID of the affix list into a leaf node.
      // In this case, we need to store the remaining affix IDs and a
      // terminator into a supplimentary list.
      supplimentary_size = node->affix_indices.size() * 2;
    }

    if (node->leaf_addition.empty()) {
      node->storage = DicNode::LEAF;
      return 2 + supplimentary_size;
    }
    node->storage = DicNode::LEAFMORE;
    // Signature & affix (2) + null for leaf_addition (1) = 3
    return 3 + node->leaf_addition.size() + supplimentary_size;
  }

  // Recursively compute the size of the children for non-leaf nodes.
  size_t child_size = 0;
  for (size_t i = 0; i < node->children.size(); i++)
    child_size += ComputeTrieStorage(node->children[i]);

  // Fixed size is only 1 byte which is the ID byte and the count combined.
  static const int kListHeaderSize = 1;

  // Lists can only store up to 16 items.
  static const size_t kListThreshold = 16;
  if (node->children.size() < kListThreshold && child_size <= 0xFF) {
    node->storage = DicNode::LIST8;
    return kListHeaderSize + node->children.size() * 2 + child_size;
  }

  if (node->children.size() < kListThreshold && child_size <= 0xFFFF) {
    node->storage = DicNode::LIST16;
    // Fixed size is one byte plus 3 for each table entry.
    return kListHeaderSize + node->children.size() * 3 + child_size;
  }

  static const int kTableHeaderSize = 2;  // Type + table size.

  bool has_0th_item;
  int first_table_item, table_item_count;
  ComputeLookupStrategyDetails(node->children, &has_0th_item,
                               &first_table_item, &table_item_count);
  if (child_size + kTableHeaderSize + (has_0th_item ? 2 : 0) +
      table_item_count * 2 < 0xFFFF) {
    // Use 16-bit addressing since the children will fit.
    node->storage = DicNode::LOOKUP16;
    return kTableHeaderSize + (has_0th_item ? 2 : 0) + table_item_count * 2 +
        child_size;
  }

  // Use 32-bit addressing as a last resort.
  node->storage = DicNode::LOOKUP32;
  return kTableHeaderSize + (has_0th_item ? 4 : 0) + table_item_count * 4 +
      child_size;
}

// Serializes the given node when it is DicNode::LEAF* to the output.
void SerializeLeaf(const DicNode* node, std::string* output) {
  // The low 6 bits of the ID byte are the high 6 bits of the first affix ID.
  int first_affix = node->affix_indices.size() ? node->affix_indices[0] : 0;

  // We may store the first value with the node or in the supplimentary list.
  size_t first_affix_in_supplimentary_list = 1;
  if (first_affix > BDict::LEAF_NODE_MAX_FIRST_AFFIX_ID) {
    // There are not enough bits for this value, move it to the supplimentary
    // list where there are more bits per value.
    first_affix_in_supplimentary_list = 0;
    first_affix = BDict::FIRST_AFFIX_IS_UNUSED;
  }

  unsigned char id_byte = (first_affix >> 8) &
      BDict::LEAF_NODE_FIRST_BYTE_AFFIX_MASK;

  // The next two bits indicates an additional string and more affixes.
  if (node->storage == DicNode::LEAFMORE)
    id_byte |= BDict::LEAF_NODE_ADDITIONAL_VALUE;
  if (node->affix_indices.size() > 1 || first_affix_in_supplimentary_list == 0)
    id_byte |= BDict::LEAF_NODE_FOLLOWING_VALUE;
  output->push_back(id_byte);

  // Following is the low 8 bits of the affix index.
  output->push_back(first_affix & 0xff);

  // Handle the optional addition with NULL terminator.
  if (node->storage == DicNode::LEAFMORE) {
    for (size_t i = 0; i < node->leaf_addition.size() + 1; i++)
      output->push_back(node->leaf_addition.c_str()[i]);
  }

  // Handle any following affixes. We already wrote the 0th one.
  if (node->affix_indices.size() > first_affix_in_supplimentary_list) {
    for (size_t i = first_affix_in_supplimentary_list;
         i < node->affix_indices.size() && i < BDict::MAX_AFFIXES_PER_WORD;
         i++) {
      output->push_back(static_cast<char>(node->affix_indices[i] & 0xFF));
      output->push_back(
          static_cast<char>((node->affix_indices[i] >> 8) & 0xFF));
    }

    // Terminator for affix list. We use 0xFFFF.
    output->push_back(static_cast<unsigned char>(0xFF));
    output->push_back(static_cast<unsigned char>(0xFF));
  }
}

// Serializes the given node when it is DicNode::LIST* to the output.
void SerializeList(const DicNode* node, std::string* output) {
  bool is_8_bit = node->storage == DicNode::LIST8;
  unsigned char id_byte = BDict::LIST_NODE_TYPE_VALUE |
      (is_8_bit ? 0 : BDict::LIST_NODE_16BIT_VALUE);
  id_byte |= node->children.size();  // We assume the size is < 4 bits.
  output->push_back(id_byte);

  // Reserve enough room for the lookup table (either 2 or 3 bytes per entry).
  int bytes_per_entry = (is_8_bit ? 2 : 3);
  size_t table_begin = output->size();
  output->resize(output->size() + node->children.size() * bytes_per_entry);
  size_t children_begin = output->size();

  for (size_t i = 0; i < node->children.size(); i++) {
    // First is the character this entry represents.
    (*output)[table_begin + i * bytes_per_entry] = node->children[i]->addition;

    // Next is the 8- or 16-bit offset.
    size_t offset = output->size() - children_begin;
    if (is_8_bit) {
      DCHECK(offset <= 0xFF);
      (*output)[table_begin + i * bytes_per_entry + 1] =
          static_cast<char>(offset & 0xFF);
    } else {
      unsigned short* output16 = reinterpret_cast<unsigned short*>(
          &(*output)[table_begin + i * bytes_per_entry + 1]);
      *output16 = static_cast<unsigned short>(offset);
    }

    // Now append the children's data.
    SerializeTrie(node->children[i], output);
  }
}

// Serializes the given node when it is DicNode::LOOKUP* to the output.
void SerializeLookup(const DicNode* node, std::string* output) {
  unsigned char id_byte = BDict::LOOKUP_NODE_TYPE_VALUE;

  bool has_0th_item;
  int first_table_item, table_item_count;
  ComputeLookupStrategyDetails(node->children, &has_0th_item,
                               &first_table_item, &table_item_count);

  // Set the extra bits in the ID byte.
  bool is_32_bit = (node->storage == DicNode::LOOKUP32);
  if (is_32_bit)
    id_byte |= BDict::LOOKUP_NODE_32BIT_VALUE;
  if (has_0th_item)
    id_byte |= BDict::LOOKUP_NODE_0TH_VALUE;

  size_t begin_offset = output->size();

  output->push_back(id_byte);
  output->push_back(static_cast<char>(first_table_item));
  output->push_back(static_cast<char>(table_item_count));

  // Save room for the lookup table and the optional 0th item.
  int bytes_per_entry = (is_32_bit ? 4 : 2);
  size_t zeroth_item_offset = output->size();
  if (has_0th_item)
    output->resize(output->size() + bytes_per_entry);
  size_t table_begin = output->size();
  output->resize(output->size() + table_item_count * bytes_per_entry);

  // Append the children.
  for (size_t i = 0; i < node->children.size(); i++) {
    size_t offset = output->size();

    // Compute the location at which we'll store the offset of the child data.
    // We may be writing the magic 0th item.
    size_t offset_offset;
    if (i == 0 && has_0th_item) {
      offset_offset = zeroth_item_offset;
    } else {
      int table_index = static_cast<unsigned char>(node->children[i]->addition) - first_table_item;
      offset_offset = table_begin + table_index * bytes_per_entry;
    }

    // Write the offset.
    if (is_32_bit) {
      // Use 32-bit absolute offsets.
      // FIXME(brettw) use bit cast.
      unsigned* offset32 = reinterpret_cast<unsigned*>(&(*output)[offset_offset]);
      *offset32 = static_cast<unsigned>(output->size());
    } else {
      // Use 16-bit relative offsets.
      unsigned short* offset16 = reinterpret_cast<unsigned short*>(&(*output)[offset_offset]);
      *offset16 = static_cast<unsigned short>(output->size() - begin_offset);
    }

    SerializeTrie(node->children[i], output);
  }
}

// Recursively serializes this node and all of its children to the output.
void SerializeTrie(const DicNode* node, std::string* output) {
  if (node->storage == DicNode::LEAF ||
      node->storage == DicNode::LEAFMORE) {
    SerializeLeaf(node, output);
  } else if (node->storage == DicNode::LIST8 ||
             node->storage == DicNode::LIST16) {
    SerializeList(node, output);
  } else if (node->storage == DicNode::LOOKUP16 ||
             node->storage == DicNode::LOOKUP32) {
    SerializeLookup(node, output);
  }
}

// Serializes the given list of strings with 0 bytes separating them. The end
// will be marked by a double-0.
void SerializeStringListNullTerm(const std::vector<std::string>& strings,
                                 std::string* output) {
  for (size_t i = 0; i < strings.size(); i++) {
    // Can't tolerate empty strings since the'll mark the end.
    if (strings[i].empty())
      output->push_back(' ');
    else
      output->append(strings[i]);
    output->push_back(0);
  }
  output->push_back(0);
}

void SerializeReplacements(
    const std::vector< std::pair<std::string, std::string> >& repl,
    std::string* output) {
  for (size_t i = 0; i < repl.size(); i++) {
    output->append(repl[i].first);
    output->push_back(0);
    output->append(repl[i].second);
    output->push_back(0);
  }
  output->push_back(0);
}

}  // namespace

BDictWriter::BDictWriter() : trie_root_(NULL) {
}

BDictWriter::~BDictWriter() {
  delete trie_root_;
}

void BDictWriter::SetWords(const WordList& words) {
  trie_root_ = new DicNode;
  BuildTrie(words, 0, words.size(), 0, trie_root_);
}

std::string BDictWriter::GetBDict() const {
  std::string ret;

  // Save room for the header. This will be populated at the end.
  ret.resize(sizeof(hunspell::BDict::Header));

  // Serialize the affix portion.
  size_t aff_offset = ret.size();
  SerializeAff(&ret);

  // Serialize the dictionary words.
  size_t dic_offset = ret.size();
  ret.reserve(ret.size() + ComputeTrieStorage(trie_root_));
  SerializeTrie(trie_root_, &ret);

  // Fill the header last, now that we have the data.
  hunspell::BDict::Header* header =
      reinterpret_cast<hunspell::BDict::Header*>(&ret[0]);
  header->signature = hunspell::BDict::SIGNATURE;
  header->major_version = hunspell::BDict::MAJOR_VERSION;
  header->minor_version = hunspell::BDict::MINOR_VERSION;
  header->aff_offset = static_cast<uint32_t>(aff_offset);
  header->dic_offset = static_cast<uint32_t>(dic_offset);

  // Write the MD5 digest of the affix information and the dictionary words at
  // the end of the BDic header.
  if (header->major_version >= 2)
    base::MD5Sum(base::as_byte_span(ret).subspan(aff_offset), &header->digest);

  return ret;
}

void BDictWriter::SerializeAff(std::string* output) const {
  // Reserve enough room for the header.
  size_t header_offset = output->size();
  output->resize(output->size() + sizeof(hunspell::BDict::AffHeader));

  // Write the comment.
  output->push_back('\n');
  output->append(comment_);
  output->push_back('\n');

  // We need a magic first AF line that lists the number of following ones.
  size_t affix_group_offset = output->size();
  output->append(base::StringPrintf("AF %d",
                                    static_cast<int>(affix_groups_.size())));
  output->push_back(0);
  SerializeStringListNullTerm(affix_groups_, output);

  size_t affix_rule_offset = output->size();
  SerializeStringListNullTerm(affix_rules_, output);

  size_t rep_offset = output->size();
  SerializeReplacements(replacements_, output);

  size_t other_offset = output->size();
  SerializeStringListNullTerm(other_commands_, output);

  // Add the header now that we know the offsets.
  hunspell::BDict::AffHeader* header =
      reinterpret_cast<hunspell::BDict::AffHeader*>(&(*output)[header_offset]);
  header->affix_group_offset = static_cast<uint32_t>(affix_group_offset);
  header->affix_rule_offset = static_cast<uint32_t>(affix_rule_offset);
  header->rep_offset = static_cast<uint32_t>(rep_offset);
  header->other_offset = static_cast<uint32_t>(other_offset);
}

}  // namespace hunspell
