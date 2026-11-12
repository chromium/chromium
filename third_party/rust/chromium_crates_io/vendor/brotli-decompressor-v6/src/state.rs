#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(non_upper_case_globals)]


use alloc;
use core;
use context::kContextLookup;
use bit_reader::{BrotliBitReader, BrotliGetAvailableBits, BrotliInitBitReader};
use huffman::{BROTLI_HUFFMAN_MAX_CODE_LENGTH, BROTLI_HUFFMAN_MAX_CODE_LENGTHS_SIZE,
              BROTLI_HUFFMAN_MAX_SIZE_272, HuffmanCode, HuffmanTreeGroup};
use alloc::SliceWrapper;
use alloc::SliceWrapperMut;
use shared_dictionary;
use shared_dictionary::{BrotliSharedDictionary, TRANSFORM_LIST_STRIDE, WORD_LIST_STRIDE};

#[allow(dead_code)]
pub enum WhichTreeGroup {
  LITERAL,
  INSERT_COPY,
  DISTANCE,
}
// Options for BrotliState::set_parameter and the C API's
// BrotliDecoderSetParameter, with the C header's values.
#[repr(C)]
#[derive(Clone,Copy, Debug)]
pub enum BrotliDecoderParameter {
  BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION = 0,
  BROTLI_DECODER_PARAM_LARGE_WINDOW = 1,
}
#[repr(C)]
#[derive(Clone,Copy, Debug)]
pub enum BrotliDecoderErrorCode{
  BROTLI_DECODER_NO_ERROR = 0,
  /* Same as BrotliDecoderResult values */
  BROTLI_DECODER_SUCCESS = 1,
  BROTLI_DECODER_NEEDS_MORE_INPUT = 2,
  BROTLI_DECODER_NEEDS_MORE_OUTPUT = 3,

  /* Errors caused by invalid input */
  BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_NIBBLE = -1,
  BROTLI_DECODER_ERROR_FORMAT_RESERVED = -2,
  BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_META_NIBBLE = -3,
  BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_ALPHABET = -4,
  BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_SAME = -5,
  BROTLI_DECODER_ERROR_FORMAT_CL_SPACE = -6,
  BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE = -7,
  BROTLI_DECODER_ERROR_FORMAT_CONTEXT_MAP_REPEAT = -8,
  BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_1 = -9,
  BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_2 = -10,
  BROTLI_DECODER_ERROR_FORMAT_TRANSFORM = -11,
  BROTLI_DECODER_ERROR_FORMAT_DICTIONARY = -12,
  BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS = -13,
  BROTLI_DECODER_ERROR_FORMAT_PADDING_1 = -14,
  BROTLI_DECODER_ERROR_FORMAT_PADDING_2 = -15,
  BROTLI_DECODER_ERROR_FORMAT_DISTANCE = -16,
  BROTLI_DECODER_ERROR_FORMAT_BLOCK_SWITCH = -17,

  BROTLI_DECODER_ERROR_COMPOUND_DICTIONARY = -18,
  BROTLI_DECODER_ERROR_DICTIONARY_NOT_SET = -19,
  BROTLI_DECODER_ERROR_INVALID_ARGUMENTS = -20,

  /* Memory allocation problems */
  BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MODES = -21,
  /* Literal = insert and distance trees together */
  BROTLI_DECODER_ERROR_ALLOC_TREE_GROUPS = -22,
  /* -23..-24 codes are reserved for distinct tree groups */
  BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MAP = -25,
  BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_1 = -26,
  BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_2 = -27,
  /* -28..-29 codes are reserved for dynamic ring-buffer allocation */
  BROTLI_DECODER_ERROR_ALLOC_BLOCK_TYPE_TREES = -30,

  /* "Impossible" states */
  BROTLI_DECODER_ERROR_UNREACHABLE = -31,
}

#[derive(Debug)]
pub enum BrotliRunningState {
  BROTLI_STATE_UNINITED,
  BROTLI_STATE_LARGE_WINDOW_BITS,
  BROTLI_STATE_INITIALIZE,
  BROTLI_STATE_METABLOCK_BEGIN,
  BROTLI_STATE_METABLOCK_HEADER,
  BROTLI_STATE_METABLOCK_HEADER_2,
  BROTLI_STATE_CONTEXT_MODES,
  BROTLI_STATE_COMMAND_BEGIN,
  BROTLI_STATE_COMMAND_INNER,
  BROTLI_STATE_COMMAND_POST_DECODE_LITERALS,
  BROTLI_STATE_COMMAND_POST_WRAP_COPY,
  BROTLI_STATE_UNCOMPRESSED,
  BROTLI_STATE_METADATA,
  BROTLI_STATE_COMMAND_INNER_WRITE,
  BROTLI_STATE_METABLOCK_DONE,
  BROTLI_STATE_COMMAND_POST_WRITE_1,
  BROTLI_STATE_COMMAND_POST_WRITE_2,
  BROTLI_STATE_HUFFMAN_CODE_0,
  BROTLI_STATE_HUFFMAN_CODE_1,
  BROTLI_STATE_HUFFMAN_CODE_2,
  BROTLI_STATE_HUFFMAN_CODE_3,
  BROTLI_STATE_CONTEXT_MAP_1,
  BROTLI_STATE_CONTEXT_MAP_2,
  BROTLI_STATE_TREE_GROUP,
  BROTLI_STATE_DONE,
}

pub enum BrotliRunningMetablockHeaderState {
  BROTLI_STATE_METABLOCK_HEADER_NONE,
  BROTLI_STATE_METABLOCK_HEADER_EMPTY,
  BROTLI_STATE_METABLOCK_HEADER_NIBBLES,
  BROTLI_STATE_METABLOCK_HEADER_SIZE,
  BROTLI_STATE_METABLOCK_HEADER_UNCOMPRESSED,
  BROTLI_STATE_METABLOCK_HEADER_RESERVED,
  BROTLI_STATE_METABLOCK_HEADER_BYTES,
  BROTLI_STATE_METABLOCK_HEADER_METADATA,
}
pub enum BrotliRunningUncompressedState {
  BROTLI_STATE_UNCOMPRESSED_NONE,
  BROTLI_STATE_UNCOMPRESSED_WRITE,
}

pub enum BrotliRunningTreeGroupState {
  BROTLI_STATE_TREE_GROUP_NONE,
  BROTLI_STATE_TREE_GROUP_LOOP,
}

pub enum BrotliRunningContextMapState {
  BROTLI_STATE_CONTEXT_MAP_NONE,
  BROTLI_STATE_CONTEXT_MAP_READ_PREFIX,
  BROTLI_STATE_CONTEXT_MAP_HUFFMAN,
  BROTLI_STATE_CONTEXT_MAP_DECODE,
  BROTLI_STATE_CONTEXT_MAP_TRANSFORM,
}

pub enum BrotliRunningHuffmanState {
  BROTLI_STATE_HUFFMAN_NONE,
  BROTLI_STATE_HUFFMAN_SIMPLE_SIZE,
  BROTLI_STATE_HUFFMAN_SIMPLE_READ,
  BROTLI_STATE_HUFFMAN_SIMPLE_BUILD,
  BROTLI_STATE_HUFFMAN_COMPLEX,
  BROTLI_STATE_HUFFMAN_LENGTH_SYMBOLS,
}

pub enum BrotliRunningDecodeUint8State {
  BROTLI_STATE_DECODE_UINT8_NONE,
  BROTLI_STATE_DECODE_UINT8_SHORT,
  BROTLI_STATE_DECODE_UINT8_LONG,
}

pub enum BrotliRunningReadBlockLengthState {
  BROTLI_STATE_READ_BLOCK_LENGTH_NONE,
  BROTLI_STATE_READ_BLOCK_LENGTH_SUFFIX,
}

pub const kLiteralContextBits: usize = 6;

// Maximum number of compound dictionary chunks that may be attached to a
// decoder instance, per the shared-brotli draft
// (https://datatracker.ietf.org/doc/draft-vandevenne-shared-brotli-format/).
pub const SHARED_BROTLI_MAX_COMPOUND_DICTS: usize = 15;
// Practical limit for the total size of attached raw dictionaries; matches
// SHARED_BROTLI_MAX_RAW_DICT_SIZE in the C implementation.
#[cfg(target_pointer_width = "64")]
pub const SHARED_BROTLI_MAX_RAW_DICT_SIZE: usize = 1usize << 31;
#[cfg(not(target_pointer_width = "64"))]
pub const SHARED_BROTLI_MAX_RAW_DICT_SIZE: usize = 1usize << 27;

// Backing store for an attached dictionary: either memory this decoder owns
// (allocated through AllocU8, freed on cleanup) or memory the caller owns and
// has promised to keep alive and unmodified for as long as the decoder lives.
//
// The borrowed variant exists so that the FFI can honor the C API's contract
// -- "Attached dictionary ownership is not transferred. Data provided to this
// method should be kept accessible until decoding is finished and decoder
// instance is destroyed." (c/include/brotli/decode.h) -- which lets many
// decoder instances share one dictionary image at zero marginal cost instead
// of each holding a private copy.
#[allow(dead_code)]
pub enum MaybeOwnedSlice<AllocU8: alloc::Allocator<u8>> {
  Owned(AllocU8::AllocatedMemory),
  // The 'static here is a lie maintained by the unsafe constructors below:
  // the referent only has to outlive the BrotliState that holds it. It is
  // spelled 'static rather than as a raw pointer so that BrotliState keeps
  // its automatic Send/Sync.
  Borrowed(&'static [u8]),
  // A dictionary of this length whose bytes are lent to every
  // BrotliDecompressStreamWithDictionary call instead.
  External(usize),
}

impl<AllocU8: alloc::Allocator<u8>> Default for MaybeOwnedSlice<AllocU8> {
  fn default() -> Self {
    MaybeOwnedSlice::Owned(AllocU8::AllocatedMemory::default())
  }
}

impl<AllocU8: alloc::Allocator<u8>> alloc::SliceWrapper<u8> for MaybeOwnedSlice<AllocU8> {
  fn slice(&self) -> &[u8] {
    match *self {
      MaybeOwnedSlice::Owned(ref mem) => mem.slice(),
      MaybeOwnedSlice::Borrowed(data) => data,
      MaybeOwnedSlice::External(_) => &[],
    }
  }
}

// Sentinel stored in BrotliDecoderCompoundDictionary::block_bits while the
// block map is stale, i.e. a marker rather than a shift amount. Real shift
// amounts computed by EnsureCompoundDictionaryInitialized are in 0..=23
// (total_size is capped at SHARED_BROTLI_MAX_RAW_DICT_SIZE), so this value can
// never collide with one. The C implementation uses -1 for the same purpose;
// the field is unsigned here.
pub const COMPOUND_DICTIONARY_BLOCK_MAP_UNINITIALIZED: u32 = 255;


pub trait BrotliMetadataCallback {
    fn start(&mut self, _size: usize) {}
    fn chunk(&mut self, _data: &[u8]) {}
}

// LZ77 prefix ("compound") dictionaries attached to the decoder. Unlike the
// historical approach of copying the dictionary into the ring buffer, chunks
// are kept in their own buffers and referenced by backward distances in
// (max_distance, max_distance + total_size], so they stay addressable even
// after the ring buffer wraps (issue #42).
pub struct BrotliDecoderCompoundDictionary<AllocU8: alloc::Allocator<u8>> {
  pub num_chunks: usize,
  pub total_size: usize,
  // Map from address >> block_bits to the first chunk that might contain that
  // address; COMPOUND_DICTIONARY_BLOCK_MAP_UNINITIALIZED means "not yet
  // computed".
  pub block_bits: u32,
  pub block_map: [u8; 256],
  // chunk_offsets[i] is the address of the first byte of chunk i;
  // chunk_offsets[num_chunks] == total_size.
  pub chunk_offsets: [u32; SHARED_BROTLI_MAX_COMPOUND_DICTS + 1],
  pub chunks: [MaybeOwnedSlice<AllocU8>; SHARED_BROTLI_MAX_COMPOUND_DICTS],
  // Cursor for an in-progress copy from the dictionary into the ring buffer:
  // a single copy command may be interrupted to flush the ring buffer.
  pub br_index: usize,
  pub br_offset: usize,
  pub br_length: usize,
  pub br_copied: usize,
}

impl<AllocU8: alloc::Allocator<u8>> Default for BrotliDecoderCompoundDictionary<AllocU8> {
  fn default() -> Self {
    BrotliDecoderCompoundDictionary::<AllocU8> {
      num_chunks: 0,
      total_size: 0,
      block_bits: COMPOUND_DICTIONARY_BLOCK_MAP_UNINITIALIZED,
      block_map: [0; 256],
      chunk_offsets: [0; SHARED_BROTLI_MAX_COMPOUND_DICTS + 1],
      chunks: Default::default(),
      br_index: 0,
      br_offset: 0,
      br_length: 0,
      br_copied: 0,
    }
  }
}

pub struct BlockTypeAndLengthState<AllocHC: alloc::Allocator<HuffmanCode>> {
  pub substate_read_block_length: BrotliRunningReadBlockLengthState,
  pub num_block_types: [u32; 3],
  pub block_length_index: u32,
  pub block_length: [u32; 3],
  pub block_type_trees: AllocHC::AllocatedMemory,
  pub block_len_trees: AllocHC::AllocatedMemory,
  pub block_type_rb: [u32; 6],
}

pub struct BrotliState<AllocU8: alloc::Allocator<u8>,
                       AllocU32: alloc::Allocator<u32>,
                       AllocHC: alloc::Allocator<HuffmanCode>>
{
  pub state: BrotliRunningState,

  // This counter is reused for several disjoint loops.
  pub loop_counter: i32,
  pub br: BrotliBitReader,
  pub alloc_u8: AllocU8,
  pub alloc_u32: AllocU32,
  pub alloc_hc: AllocHC,
  // void* memory_manager_opaque,
  pub buffer: [u8; 8],
  pub buffer_length: u32,
  pub pos: i32,
  pub max_backward_distance: i32,
  pub max_distance: i32,
  pub ringbuffer_size: i32,
  pub ringbuffer_mask: i32,
  // Size the ring buffer must grow to before the current metablock is
  // decoded, as computed from its header by BrotliCalculateRingBufferSize.
  // It differs from ringbuffer_size only until BrotliEnsureRingBuffer runs.
  pub(crate) new_ringbuffer_size: i32,
  // Size of the pool that alloc_u8 draws from when it is a fixed pool that
  // never merges freed blocks, as in brotli_decode_prealloc; 0 otherwise.
  // Growing the ring buffer takes up to twice the window from such a pool, so
  // BrotliCalculateRingBufferSize then sizes it to never need more of the pool
  // than before the ring buffer grew on demand.
  pub(crate) u8_pool_size: usize,
  // Smallest size the ring buffer starts from, as set by
  // set_initial_ring_buffer_size; 0 for the default.
  pub(crate) initial_ringbuffer_size: u32,
  // Output capacity of a one-shot decode. This is only a speculative sizing
  // hint: a caller may provide much more space than the stream needs.
  pub(crate) one_shot_output_size: u32,
  pub dist_rb_idx: i32,
  pub dist_rb: [i32; 4],
  pub ringbuffer: AllocU8::AllocatedMemory,
  // pub ringbuffer_end : usize,
  pub htree_command_index: u16,
  pub context_lookup: &'static [u8;512],
  pub context_map_slice_index: usize,
  pub dist_context_map_slice_index: usize,

  pub sub_loop_counter: u32,

  // This ring buffer holds a few past copy distances that will be used by */
  // some special distance codes.
  pub literal_hgroup: HuffmanTreeGroup<AllocU32, AllocHC>,
  pub insert_copy_hgroup: HuffmanTreeGroup<AllocU32, AllocHC>,
  pub distance_hgroup: HuffmanTreeGroup<AllocU32, AllocHC>,
  // This is true if the literal context map histogram type always matches the
  // block type. It is then not needed to keep the context (faster decoding).
  pub trivial_literal_context: i32,
  // Distance context is actual after command is decoded and before distance
  // is computed. After distance computation it is used as a temporary variable
  pub distance_context: i32,
  pub meta_block_remaining_len: i32,
  pub block_type_length_state: BlockTypeAndLengthState<AllocHC>,
  pub distance_postfix_bits: u32,
  pub num_direct_distance_codes: u32,
  pub distance_postfix_mask: i32,
  pub num_dist_htrees: u32,
  pub dist_context_map: AllocU8::AllocatedMemory,
  // NOT NEEDED? the index below seems to supersede it pub literal_htree : AllocHC::AllocatedMemory,
  pub literal_htree_index: u8,
  pub dist_htree_index: u8,
  pub large_window: bool,
  pub(crate) canny_ringbuffer_allocation: bool,
  pub should_wrap_ringbuffer: bool,
  pub error_code: BrotliDecoderErrorCode,
  pub repeat_code_len: u32,
  pub prev_code_len: u32,

  pub copy_length: i32,
  pub distance_code: i32,

  // For partial write operations
  // u64, not usize: as usize these would wrap after 4GiB of output on a 32-bit
  // target, and WriteRingBuffer works from their difference.
  pub rb_roundtrips: u64, // How many times we went around the ringbuffer
  pub partial_pos_out: u64, // How much output to the user in total (<= rb)

  // For ReadHuffmanCode
  pub symbol: u32,
  pub repeat: u32,
  pub space: u32,

  pub table: [HuffmanCode; 32],
  // List of of symbol chains.
  pub symbol_lists_index: usize, // AllocU16::AllocatedMemory,
  // Storage from symbol_lists.
  pub symbols_lists_array: [u16; BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1 +
                                 BROTLI_HUFFMAN_MAX_CODE_LENGTHS_SIZE],
  // Tails of symbol chains.
  pub next_symbol: [i32; 32],
  pub code_length_code_lengths: [u8; 18],
  // Population counts for the code lengths
  pub code_length_histo: [u16; 16],

  // For HuffmanTreeGroupDecode
  pub htree_index: i32,
  pub htree_next_offset: u32,

  // For DecodeContextMap
  pub context_index: u32,
  pub max_run_length_prefix: u32,
  pub code: u32,
  // always pre-allocated on state creation
  pub context_map_table: AllocHC::AllocatedMemory,

  // For InverseMoveToFrontTransform
  pub mtf_upper_bound: u32,
  pub mtf_or_error_string: Result<[u8; 256], [u8; 256]>,

  // For custom dictionaries
  pub custom_dict: AllocU8::AllocatedMemory,
  pub compound_dictionary: BrotliDecoderCompoundDictionary<AllocU8>,
  // Custom word/transform lists from an attached serialized shared
  // dictionary; selects the built-in static dictionary when empty.
  pub dictionary: BrotliSharedDictionary<AllocU8, AllocU32>,
  // less used attributes are in the end of this struct */
  // States inside function calls
  pub substate_metablock_header: BrotliRunningMetablockHeaderState,
  pub substate_tree_group: BrotliRunningTreeGroupState,
  pub substate_context_map: BrotliRunningContextMapState,
  pub substate_uncompressed: BrotliRunningUncompressedState,
  pub substate_huffman: BrotliRunningHuffmanState,
  pub substate_decode_uint8: BrotliRunningDecodeUint8State,

  pub is_last_metablock: u8,
  pub is_uncompressed: u8,
  pub is_metadata: u8,
  pub size_nibbles: u8,
  pub window_bits: u32,

  pub num_literal_htrees: u32,
  pub context_map: AllocU8::AllocatedMemory,
  pub context_modes: AllocU8::AllocatedMemory,
  pub trivial_literal_contexts: [u32; 8],
}
macro_rules! make_brotli_state {
 ($alloc_u8 : expr, $alloc_u32 : expr, $alloc_hc : expr, $custom_dict : expr) => (BrotliState::<AllocU8, AllocU32, AllocHC>{
            state : BrotliRunningState::BROTLI_STATE_UNINITED,
            loop_counter : 0,
            br : BrotliBitReader::default(),
            alloc_u8 : $alloc_u8,
            alloc_u32 : $alloc_u32,
            alloc_hc : $alloc_hc,
            buffer : [0u8; 8],
            buffer_length : 0,
            pos : 0,
            max_backward_distance : 0,
            max_distance : 0,
            ringbuffer_size : 0,
            ringbuffer_mask: 0,
            new_ringbuffer_size : 0,
            u8_pool_size : 0,
            initial_ringbuffer_size : 0,
            one_shot_output_size : 0,
            dist_rb_idx : 0,
            dist_rb : [16, 15, 11, 4],
            ringbuffer : AllocU8::AllocatedMemory::default(),
            htree_command_index : 0,
            context_lookup : &kContextLookup[0],
            context_map_slice_index : 0,
            dist_context_map_slice_index : 0,
            sub_loop_counter : 0,

            literal_hgroup : HuffmanTreeGroup::<AllocU32, AllocHC>::default(),
            insert_copy_hgroup : HuffmanTreeGroup::<AllocU32, AllocHC>::default(),
            distance_hgroup : HuffmanTreeGroup::<AllocU32, AllocHC>::default(),
            trivial_literal_context : 0,
            distance_context : 0,
            meta_block_remaining_len : 0,
            block_type_length_state : BlockTypeAndLengthState::<AllocHC> {
              block_length_index : 0,
              block_length : [0; 3],
              num_block_types : [0;3],
              block_type_rb: [0;6],
              substate_read_block_length : BrotliRunningReadBlockLengthState::BROTLI_STATE_READ_BLOCK_LENGTH_NONE,
              block_type_trees : AllocHC::AllocatedMemory::default(),
              block_len_trees : AllocHC::AllocatedMemory::default(),
            },
            distance_postfix_bits : 0,
            num_direct_distance_codes : 0,
            distance_postfix_mask : 0,
            num_dist_htrees : 0,
            dist_context_map : AllocU8::AllocatedMemory::default(),
            //// not needed literal_htree : AllocHC::AllocatedMemory::default(),
            literal_htree_index : 0,
            dist_htree_index : 0,
            repeat_code_len : 0,
            prev_code_len : 0,
            copy_length : 0,
            distance_code : 0,
            rb_roundtrips : 0,  /* How many times we went around the ringbuffer */
            partial_pos_out : 0,  /* How much output to the user in total (<= rb) */
            symbol : 0,
            repeat : 0,
            space : 0,
            table : [HuffmanCode::default(); 32],
            //symbol_lists: AllocU16::AllocatedMemory::default(),
            symbol_lists_index : BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1,
            symbols_lists_array : [0;BROTLI_HUFFMAN_MAX_CODE_LENGTH + 1 +
                              BROTLI_HUFFMAN_MAX_CODE_LENGTHS_SIZE],
            next_symbol : [0; 32],
            code_length_code_lengths : [0; 18],
            code_length_histo : [0; 16],
            htree_index : 0,
            htree_next_offset : 0,

            /* For DecodeContextMap */
           context_index : 0,
           max_run_length_prefix : 0,
           code : 0,
           context_map_table : AllocHC::AllocatedMemory::default(),

           /* For InverseMoveToFrontTransform */
           mtf_upper_bound : 255,
           mtf_or_error_string : Ok([0; 256]),

           /* For custom dictionaries */
           custom_dict : $custom_dict,
           compound_dictionary : BrotliDecoderCompoundDictionary::default(),
           dictionary : BrotliSharedDictionary::default(),
           /* less used attributes are in the end of this struct */
           /* States inside function calls */
           substate_metablock_header : BrotliRunningMetablockHeaderState::BROTLI_STATE_METABLOCK_HEADER_NONE,
           substate_tree_group : BrotliRunningTreeGroupState::BROTLI_STATE_TREE_GROUP_NONE,
           substate_context_map : BrotliRunningContextMapState::BROTLI_STATE_CONTEXT_MAP_NONE,
           substate_uncompressed : BrotliRunningUncompressedState::BROTLI_STATE_UNCOMPRESSED_NONE,
           substate_huffman : BrotliRunningHuffmanState::BROTLI_STATE_HUFFMAN_NONE,
           substate_decode_uint8 : BrotliRunningDecodeUint8State::BROTLI_STATE_DECODE_UINT8_NONE,

           is_last_metablock : 0,
           is_uncompressed : 0,
           is_metadata : 0,
           size_nibbles : 0,
           window_bits : 0,
           large_window: false,
           canny_ringbuffer_allocation: true,
           should_wrap_ringbuffer: false,
           error_code: BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS,
           num_literal_htrees : 0,
           context_map : AllocU8::AllocatedMemory::default(),
           context_modes : AllocU8::AllocatedMemory::default(),
           trivial_literal_contexts : [0u32; 8],
        }
    );
}
impl <'brotli_state,
      AllocU8 : alloc::Allocator<u8>,
      AllocU32 : alloc::Allocator<u32>,
      AllocHC : alloc::Allocator<HuffmanCode> > BrotliState<AllocU8, AllocU32, AllocHC> {
    pub fn new(alloc_u8 : AllocU8,
           alloc_u32 : AllocU32,
           alloc_hc : AllocHC) -> Self{
        let mut retval = make_brotli_state!(alloc_u8, alloc_u32, alloc_hc, AllocU8::AllocatedMemory::default());
        retval.large_window = true;
        BrotliInitBitReader(&mut retval.br);
        retval
    }
    pub fn new_with_custom_dictionary(alloc_u8 : AllocU8,
           alloc_u32 : AllocU32,
           alloc_hc : AllocHC,
           custom_dict: AllocU8::AllocatedMemory) -> Self{
        let custom_dict_len = custom_dict.slice().len();
        let mut retval = make_brotli_state!(alloc_u8, alloc_u32, alloc_hc, custom_dict);
        retval.large_window =  true;
        BrotliInitBitReader(&mut retval.br);
        // The dictionary becomes the furthest compound dictionary chunk;
        // any subsequently attach_dictionary'd ones are nearer in distance
        // space.
        if custom_dict_len != 0 && custom_dict_len <= SHARED_BROTLI_MAX_RAW_DICT_SIZE {
            let dict = core::mem::replace(&mut retval.custom_dict,
                                          AllocU8::AllocatedMemory::default());
            // A fresh state has no chunks, so the size check above makes this
            // infallible. Oversized dictionaries remain in `custom_dict` and
            // are rejected through the normal decoder error path at startup.
            let attached = retval.attach_compound_dictionary_chunk(MaybeOwnedSlice::Owned(dict));
            debug_assert!(attached);
        }
        retval
    }
    pub fn new_strict(alloc_u8 : AllocU8,
           alloc_u32 : AllocU32,
           alloc_hc : AllocHC) -> Self{
        let mut retval = make_brotli_state!(alloc_u8, alloc_u32, alloc_hc, AllocU8::AllocatedMemory::default());
        retval.large_window =  false;
        BrotliInitBitReader(&mut retval.br);
        retval
    }
    // Sets a decoder parameter, the equivalent of the C API
    // BrotliDecoderSetParameter: a nonzero value disables ring buffer
    // reallocation (the whole window is allocated at once) or accepts
    // large-window streams. Allowed only before any compressed data has been
    // processed; returns false, changing nothing, afterwards.
    pub fn set_parameter(self : &mut Self,
                         param: BrotliDecoderParameter,
                         value: u32) -> bool {
        match self.state {
            BrotliRunningState::BROTLI_STATE_UNINITED => {},
            _ => return false,
        }
        match param {
            BrotliDecoderParameter::BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION =>
                self.canny_ringbuffer_allocation = value == 0,
            BrotliDecoderParameter::BROTLI_DECODER_PARAM_LARGE_WINDOW =>
                self.large_window = value != 0,
        }
        true
    }
    // Sets the smallest ring buffer the decoder starts from, rounded up to a
    // power of two of at least 32 bytes and capped at the stream's window,
    // which is only known once decoding starts; 0 restores the default of
    // 1 KiB. With u32::MAX the whole window is allocated at once and never
    // regrown, so a u8 allocator that is a fixed pool that never merges freed
    // blocks needs only the window from it, rather than up to twice that. If
    // the stream's first metablock of data is its last, the ring buffer is
    // still sized to just hold it, as it is for small or incompressible
    // input, which encoders store as one uncompressed metablock and an
    // empty last one, when that empty metablock is already in the input as
    // the uncompressed one starts. Disabling ring buffer reallocation with
    // set_parameter overrides this. Allowed only before any compressed data
    // has been processed; returns false, changing nothing, afterwards.
    pub fn set_initial_ring_buffer_size(self : &mut Self, size: u32) -> bool {
        match self.state {
            BrotliRunningState::BROTLI_STATE_UNINITED => {},
            _ => return false,
        }
        self.initial_ringbuffer_size = size;
        true
    }
    // Attaches a raw LZ77 prefix dictionary, the equivalent of the C API
    // BrotliDecoderAttachDictionary with BROTLI_SHARED_DICTIONARY_RAW.
    // Up to SHARED_BROTLI_MAX_COMPOUND_DICTS dictionaries may be attached,
    // only before any compressed data has been processed. The most recently
    // attached dictionary is the closest in backward-distance space.
    // Returns false (and frees the dictionary) if it cannot be attached.
    pub fn attach_dictionary(self : &mut Self,
                             dict: AllocU8::AllocatedMemory) -> bool {
        self.attach_dictionary_chunk(MaybeOwnedSlice::Owned(dict))
    }
    // As attach_dictionary, but the decoder only borrows `dict`: nothing is
    // copied and nothing is freed on cleanup, so one dictionary image can back
    // any number of concurrent decoder instances.
    //
    // The 'static bound is what makes this safe without a lifetime parameter
    // on BrotliState: the data provably outlives the decoder. Callers holding
    // a shorter-lived buffer can leak it (Box::leak, a leaked mmap) to obtain
    // one, or use attach_dictionary and pay for the copy.
    pub fn attach_dictionary_borrowed(self : &mut Self, dict: &'static [u8]) -> bool {
        self.attach_dictionary_chunk(MaybeOwnedSlice::Borrowed(dict))
    }
    // As attach_dictionary_borrowed, for a dictionary of any lifetime: only
    // its size is attached, and its bytes are lent to every
    // BrotliDecompressStreamWithDictionary call instead, so they only have to
    // outlive each call, not the decoder. At most one dictionary can be
    // attached this way.
    pub fn attach_external_dictionary(self : &mut Self, size: usize) -> bool {
        if self.external_dictionary_len() != 0 {
            return false;
        }
        self.attach_dictionary_chunk(MaybeOwnedSlice::External(size))
    }
    // The size of the external dictionary every call must lend, or 0.
    pub(crate) fn external_dictionary_len(&self) -> usize {
        let addon = &self.compound_dictionary;
        for chunk in addon.chunks.iter().take(addon.num_chunks) {
            if let MaybeOwnedSlice::External(len) = *chunk {
                return len;
            }
        }
        0
    }
    fn attach_dictionary_chunk(self : &mut Self,
                               dict: MaybeOwnedSlice<AllocU8>) -> bool {
        match self.state {
            BrotliRunningState::BROTLI_STATE_UNINITED => {},
            _ => {
                self.free_chunk(dict);
                return false;
            },
        }
        self.attach_compound_dictionary_chunk(dict)
    }
    // Attaches a serialized shared dictionary (magic bytes 0x91 0x00), the
    // equivalent of the C API BrotliDecoderAttachDictionary with
    // BROTLI_SHARED_DICTIONARY_SERIALIZED. An embedded LZ77 prefix dictionary
    // becomes a compound dictionary chunk; custom word lists and transform
    // lists replace the built-in static dictionary. Only one attached
    // dictionary may carry custom word/transform lists. Allowed only before
    // any compressed data has been processed. Returns false (and frees the
    // dictionary) on failure.
    pub fn attach_serialized_dictionary(self : &mut Self,
                                        dict_data: AllocU8::AllocatedMemory) -> bool {
        self.attach_serialized_dictionary_chunk(MaybeOwnedSlice::Owned(dict_data))
    }
    // As attach_serialized_dictionary, but the decoder only borrows
    // `dict_data`: the blob is referenced in place and an embedded LZ77
    // prefix becomes a borrowed chunk rather than a copy. Note that custom
    // word/transform lists keep referencing the blob for the whole decode,
    // not just for parsing, which is why the 'static bound covers both.
    pub fn attach_serialized_dictionary_borrowed(self : &mut Self,
                                                 dict_data: &'static [u8]) -> bool {
        self.attach_serialized_dictionary_chunk(MaybeOwnedSlice::Borrowed(dict_data))
    }
    fn attach_serialized_dictionary_chunk(self : &mut Self,
                                          dict_data: MaybeOwnedSlice<AllocU8>) -> bool {
        match self.state {
            BrotliRunningState::BROTLI_STATE_UNINITED => {},
            _ => {
                self.free_chunk(dict_data);
                return false;
            },
        }
        let summary = match shared_dictionary::dry_parse_serialized_dictionary(
            dict_data.slice()) {
            Ok(summary) => summary,
            Err(()) => {
                self.free_chunk(dict_data);
                return false;
            },
        };
        let is_custom = summary.num_word_lists != 0 || summary.num_transform_lists != 0;
        // Cannot combine different custom static dictionaries, only prefix
        // dictionaries.
        if is_custom && self.dictionary.is_custom() {
            self.free_chunk(dict_data);
            return false;
        }
        // If the blob itself is borrowed, an embedded prefix can be handed to
        // the compound dictionary as a subslice of it; both are shared
        // references to caller-owned memory, so no copy is needed.
        let borrowed_blob: Option<&'static [u8]> = match dict_data {
            MaybeOwnedSlice::Borrowed(data) => Some(data),
            MaybeOwnedSlice::Owned(_) | MaybeOwnedSlice::External(_) => None,
        };
        let mut parsed = BrotliSharedDictionary::<AllocU8, AllocU32>::default();
        if is_custom {
            let arena_size = summary.num_word_lists as usize * WORD_LIST_STRIDE +
                             summary.num_transform_lists as usize * TRANSFORM_LIST_STRIDE;
            parsed.meta = self.alloc_u32.alloc_cell(arena_size);
            if parsed.meta.slice().len() != arena_size ||
               shared_dictionary::parse_serialized_dictionary_into(
                   dict_data.slice(), &summary, &mut parsed).is_err() {
                self.alloc_u32.free_cell(core::mem::replace(&mut parsed.meta,
                                         AllocU32::AllocatedMemory::default()));
                self.free_chunk(dict_data);
                return false;
            }
        }
        if let Some((prefix_offset, prefix_len)) = summary.prefix {
            let chunk = match borrowed_blob {
                Some(blob) => MaybeOwnedSlice::Borrowed(
                    &blob[prefix_offset..prefix_offset + prefix_len]),
                // An owned blob is not kept around unless it carries custom
                // lists, so the prefix is copied into its own buffer; that
                // also leaves the parsed word/transform offsets referencing
                // the blob unchanged.
                None => {
                    let mut chunk = self.alloc_u8.alloc_cell(prefix_len);
                    if chunk.slice().len() != prefix_len {
                        self.alloc_u8.free_cell(chunk);
                        self.alloc_u32.free_cell(core::mem::replace(&mut parsed.meta,
                                                 AllocU32::AllocatedMemory::default()));
                        self.free_chunk(dict_data);
                        return false;
                    }
                    chunk.slice_mut().clone_from_slice(
                        &dict_data.slice()[prefix_offset..prefix_offset + prefix_len]);
                    MaybeOwnedSlice::Owned(chunk)
                },
            };
            if !self.attach_compound_dictionary_chunk(chunk) {
                self.alloc_u32.free_cell(core::mem::replace(&mut parsed.meta,
                                         AllocU32::AllocatedMemory::default()));
                self.free_chunk(dict_data);
                return false;
            }
        }
        if is_custom {
            parsed.blob = dict_data;
            let old = core::mem::replace(&mut self.dictionary, parsed);
            self.free_chunk(old.blob);
            self.alloc_u32.free_cell(old.meta);
        } else {
            // Nothing references the blob: a prefix chunk, if any, either
            // borrows the caller's memory or was copied out.
            self.free_chunk(dict_data);
        }
        true
    }
    // Attaches one LZ77 prefix ("compound") dictionary chunk. Chunks must be
    // attached before any data is decompressed; the most recently attached
    // chunk is the closest in backward-distance space. Returns false (and
    // frees the chunk) if the chunk cannot be attached.
    // Releases a dictionary backing store: owned memory goes back to the
    // allocator, borrowed memory belongs to the caller and is left alone.
    pub(crate) fn free_chunk(self : &mut Self, chunk: MaybeOwnedSlice<AllocU8>) {
        if let MaybeOwnedSlice::Owned(mem) = chunk {
            self.alloc_u8.free_cell(mem);
        }
    }
    pub(crate) fn attach_compound_dictionary_chunk(self : &mut Self,
                                            chunk: MaybeOwnedSlice<AllocU8>) -> bool {
        let size = match chunk {
            MaybeOwnedSlice::External(len) => len,
            _ => chunk.slice().len(),
        };
        // A zero-length chunk is a no-op and not counted toward the limit.
        if size == 0 {
            self.free_chunk(chunk);
            return true;
        }
        let addon = &mut self.compound_dictionary;
        if addon.num_chunks == SHARED_BROTLI_MAX_COMPOUND_DICTS ||
           size > SHARED_BROTLI_MAX_RAW_DICT_SIZE - addon.total_size {
            self.free_chunk(chunk);
            return false;
        }
        addon.chunks[addon.num_chunks] = chunk;
        addon.num_chunks += 1;
        addon.total_size += size;
        addon.chunk_offsets[addon.num_chunks] = addon.total_size as u32;
        // Invalidate the lazily-computed block map.
        addon.block_bits = COMPOUND_DICTIONARY_BLOCK_MAP_UNINITIALIZED;
        true
    }
    pub fn BrotliStateMetablockBegin(self : &mut Self) {
        self.meta_block_remaining_len = 0;
        self.block_type_length_state.block_length[0] = 1u32 << 24;
        self.block_type_length_state.block_length[1] = 1u32 << 24;
        self.block_type_length_state.block_length[2] = 1u32 << 24;
        self.block_type_length_state.num_block_types[0] = 1;
        self.block_type_length_state.num_block_types[1] = 1;
        self.block_type_length_state.num_block_types[2] = 1;
        self.block_type_length_state.block_type_rb[0] = 1;
        self.block_type_length_state.block_type_rb[1] = 0;
        self.block_type_length_state.block_type_rb[2] = 1;
        self.block_type_length_state.block_type_rb[3] = 0;
        self.block_type_length_state.block_type_rb[4] = 1;
        self.block_type_length_state.block_type_rb[5] = 0;
        self.context_map_slice_index = 0;
        self.literal_htree_index = 0;
        self.dist_context_map_slice_index = 0;
        self.dist_htree_index = 0;
        self.context_lookup = &kContextLookup[0];
    }
    pub fn BrotliStateCleanupAfterMetablock(self : &mut Self) {
        self.alloc_u8.free_cell(core::mem::replace(&mut self.context_map,
                                             AllocU8::AllocatedMemory::default()));
        self.alloc_u8.free_cell(core::mem::replace(&mut self.context_modes,
                                             AllocU8::AllocatedMemory::default()));
        self.alloc_u8.free_cell(core::mem::replace(&mut self.dist_context_map,
                                             AllocU8::AllocatedMemory::default()));


        self.literal_hgroup.reset(&mut self.alloc_u32, &mut self.alloc_hc);
        self.insert_copy_hgroup.reset(&mut self.alloc_u32, &mut self.alloc_hc);
        self.distance_hgroup.reset(&mut self.alloc_u32, &mut self.alloc_hc);
    }

   fn BrotliStateCleanup(self : &mut Self) {
      self.BrotliStateCleanupAfterMetablock();
      self.alloc_u8.free_cell(core::mem::replace(&mut self.ringbuffer,
                              AllocU8::AllocatedMemory::default()));
      self.alloc_hc.free_cell(core::mem::replace(&mut self.block_type_length_state.block_type_trees,
                              AllocHC::AllocatedMemory::default()));
      self.alloc_hc.free_cell(core::mem::replace(&mut self.block_type_length_state.block_len_trees,
                              AllocHC::AllocatedMemory::default()));
      self.alloc_hc.free_cell(core::mem::replace(&mut self.context_map_table,
                              AllocHC::AllocatedMemory::default()));
      self.alloc_u8.free_cell(core::mem::replace(&mut self.custom_dict,
                              AllocU8::AllocatedMemory::default()));
      let num_chunks = self.compound_dictionary.num_chunks.min(self.compound_dictionary.chunks.len());
      for chunk in self.compound_dictionary.chunks[..num_chunks].iter_mut() {
        if let MaybeOwnedSlice::Owned(mem) =
            core::mem::replace(chunk, MaybeOwnedSlice::default()) {
          self.alloc_u8.free_cell(mem);
        }
      }
      self.compound_dictionary.num_chunks = 0;
      self.compound_dictionary.total_size = 0;
      if let MaybeOwnedSlice::Owned(mem) =
          core::mem::replace(&mut self.dictionary.blob, MaybeOwnedSlice::default()) {
        self.alloc_u8.free_cell(mem);
      }
      self.alloc_u32.free_cell(core::mem::replace(&mut self.dictionary.meta,
                               AllocU32::AllocatedMemory::default()));

      //FIXME??  BROTLI_FREE(s, s->legacy_input_buffer);
      //FIXME??  BROTLI_FREE(s, s->legacy_output_buffer);
    }

    pub fn BrotliStateIsStreamStart(self : &Self) -> bool {
        match self.state {
            BrotliRunningState::BROTLI_STATE_UNINITED =>
                BrotliGetAvailableBits(&self.br) == 0,
            _ => false,
        }
    }

    pub fn BrotliStateIsStreamEnd(self : &Self) -> bool {
        match self.state {
            BrotliRunningState::BROTLI_STATE_DONE => true,
            _ => false
        }
    }
    pub fn BrotliHuffmanTreeGroupInit(self :&mut Self, group : WhichTreeGroup,
                                      alphabet_size : u16, max_symbol: u16, ntrees : u16) {
        match group {
            WhichTreeGroup::LITERAL => self.literal_hgroup.init(&mut self.alloc_u32,
                                                                &mut self.alloc_hc,
                                                                alphabet_size, max_symbol, ntrees),
            WhichTreeGroup::INSERT_COPY => self.insert_copy_hgroup.init(&mut self.alloc_u32,
                                                                        &mut self.alloc_hc,
                                                                        alphabet_size, max_symbol, ntrees),
            WhichTreeGroup::DISTANCE => self.distance_hgroup.init(&mut self.alloc_u32,
                                                                  &mut self.alloc_hc,
                                                                  alphabet_size, max_symbol, ntrees),
        }
    }
    pub fn BrotliHuffmanTreeGroupRelease(self :&mut Self, group : WhichTreeGroup) {
        match group {
            WhichTreeGroup::LITERAL => self.literal_hgroup.reset(&mut self.alloc_u32,
                                                                 &mut self.alloc_hc),
            WhichTreeGroup::INSERT_COPY => self.insert_copy_hgroup.reset(&mut self.alloc_u32,
                                                                         &mut self.alloc_hc),
            WhichTreeGroup::DISTANCE => self.distance_hgroup.reset(&mut self.alloc_u32,
                                                                   &mut self.alloc_hc),
        }
    }
}

impl <'brotli_state,
      AllocU8 : alloc::Allocator<u8>,
      AllocU32 : alloc::Allocator<u32>,
      AllocHC : alloc::Allocator<HuffmanCode> > Drop for BrotliState<AllocU8, AllocU32, AllocHC> {
    fn drop(&mut self) {
        self.BrotliStateCleanup();
    }
}



pub fn BrotliDecoderErrorStr(c: BrotliDecoderErrorCode) -> &'static str {
  match c {
  BrotliDecoderErrorCode::BROTLI_DECODER_NO_ERROR => "NO_ERROR\0",
  /* Same as BrotliDecoderResult values */
  BrotliDecoderErrorCode::BROTLI_DECODER_SUCCESS => "SUCCESS\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_INPUT => "NEEDS_MORE_INPUT\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_NEEDS_MORE_OUTPUT => "NEEDS_MORE_OUTPUT\0",

  /* Errors caused by invalid input */
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_NIBBLE => "ERROR_FORMAT_EXUBERANT_NIBBLE\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_RESERVED => "ERROR_FORMAT_RESERVED\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_EXUBERANT_META_NIBBLE => "ERROR_FORMAT_EXUBERANT_META_NIBBLE\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_ALPHABET => "ERROR_FORMAT_SIMPLE_HUFFMAN_ALPHABET\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_SIMPLE_HUFFMAN_SAME => "ERROR_FORMAT_SIMPLE_HUFFMAN_SAME\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_CL_SPACE => "ERROR_FORMAT_FL_SPACE\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_HUFFMAN_SPACE => "ERROR_FORMAT_HUFFMAN_SPACE\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_CONTEXT_MAP_REPEAT => "ERROR_FORMAT_CONTEXT_MAP_REPEAT\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_1 =>"ERROR_FORMAT_BLOCK_LENGTH_1\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_BLOCK_LENGTH_2 =>"ERROR_FORMAT_BLOCK_LENGTH_2\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_TRANSFORM => "ERROR_FORMAT_TRANSFORM\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_DICTIONARY =>"ERROR_FORMAT_DICTIONARY\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_WINDOW_BITS =>"ERROR_FORMAT_WINDOW_BITS\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_PADDING_1 =>"ERROR_FORMAT_PADDING_1\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_PADDING_2 =>"ERROR_FORMAT_PADDING_2\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_DISTANCE =>"ERROR_FORMAT_DISTANCE\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_FORMAT_BLOCK_SWITCH =>"ERROR_FORMAT_BLOCK_SWITCH\0",

  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_COMPOUND_DICTIONARY => "ERROR_COMPOUND_DICTIONARY\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_DICTIONARY_NOT_SET => "ERROR_DICTIONARY_NOT_SET\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_INVALID_ARGUMENTS => "ERROR_INVALID_ARGUMENTS\0",

  /* Memory allocation problems */
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MODES => "ERROR_ALLOC_CONTEXT_MODES\0",
  /* Literal => insert and distance trees together */
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_TREE_GROUPS => "ERROR_ALLOC_TREE_GROUPS\0",
  /* -23..-24 codes are reserved for distinct tree groups */
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MAP => "ERROR_ALLOC_CONTEXT_MAP\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_1 => "ERROR_ALLOC_RING_BUFFER_1\0",
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_RING_BUFFER_2 => "ERROR_ALLOC_RING_BUFFER_2\0",
  /* -28..-29 codes are reserved for dynamic ring-buffer allocation */
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_ALLOC_BLOCK_TYPE_TREES => "ERROR_ALLOC_BLOCK_TYPE_TREES\0",

  /* "Impossible" states */
  BrotliDecoderErrorCode::BROTLI_DECODER_ERROR_UNREACHABLE => "ERROR_UNREACHABLE\0",
  }
}
