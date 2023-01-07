#ifndef __ZXCVBN__MATCHING_HPP
#define __ZXCVBN__MATCHING_HPP

#include <zxcvbn/common.hpp>
#include <zxcvbn/frequency_lists.hpp>
#include <zxcvbn/adjacency_graphs.hpp>

#include <string>
#include <vector>

namespace zxcvbn {

const std::vector<std::pair<std::string, std::vector<std::string>>>& L33T_TABLE();
const std::vector<std::pair<RegexTag, std::regex>>& REGEXEN();

std::vector<Match> dictionary_match(const std::string & password,
                                    const RankedDicts & ranked_dictionaries);

std::vector<Match> reverse_dictionary_match(const std::string & password,
                                            const RankedDicts & ranked_dictionaries);

std::unordered_map<std::string, std::vector<std::string>> relevant_l33t_subtable(const std::string & password, const std::vector<std::pair<std::string, std::vector<std::string>>> & table);

std::vector<std::unordered_map<std::string, std::string>> enumerate_l33t_subs(const std::unordered_map<std::string, std::vector<std::string>> & table);

std::vector<Match> l33t_match(const std::string & password,
                              const RankedDicts & ranked_dictionaries,
                              const std::vector<std::pair<std::string, std::vector<std::string>>> & l33t_table);

std::vector<Match> spatial_match(const std::string & password,
                                 const Graphs & graphs);

std::vector<Match> repeat_match(const std::string & password);

std::vector<Match> sequence_match(const std::string & password);

std::vector<Match> regex_match(const std::string & password,
                               const std::vector<std::pair<RegexTag, std::regex>> & regex);

std::vector<Match> date_match(const std::string & password);

std::vector<Match> omnimatch(const std::string & password);

}

#endif
