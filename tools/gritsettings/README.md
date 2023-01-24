### tools/gritsettings README

This directory contains several files that apply globally to the Chrome resource
generation system (which uses GRIT - see tools/grit).

**resource_ids.spec**: This file is used to assign fake start IDs for resources
and strings used by Chromium. This is done to ensure that resource ids are
unique across all `.grd` files. If you are adding a new `.grd` file, please
add a new entry to this file. Detailed instructions are found below.

**translation_expectations.pyl**: Specifies which grd files should be translated
and into which languages they should be translated. Used by the internal
translation process.

**startup_resources_[platform].txt**: These files provide a pre-determined
resource id ordering that will be used by GRIT when assigning resources ids. The
goal is to have the resource loaded during Chrome startup be ordered first in
the .pak files, so that fewer page faults are suffered during Chrome start up.
To update or generate one of these files, follow these instructions:

  1. Build a Chrome official release build and launch it with command line:
     `--print-resource-ids` and save the output to a file (e.g. res.txt).

  2. Generate the startup_resources_[platform].txt via the following command
     (you can redirect its output to the new file location):

     `
     tools/grit/grit/format/gen_predetermined_ids.py res_ids.txt out/gn
     `

     In the above command, res_ids.txt is the file produced in step 1 and out/gn
     is you Chrome build directory where you compiled Chrome. The output of the
     command can be added as a new startup_resource_[platform]

  3. If this is a new file, modify `tools/grit/grit_rule.gni` to set its path
     via `grit_predetermined_resource_ids_file` for the given platform.

#### Updating resource_ids.spec

The `[###]` integers in `resource_ids.spec` are **fake start IDs**, whose
relative ordering determine relationship among `.grd` entries. Actual resource
IDs are dynamically generated, with ID usage requirements from `.grd` files if
static, or injected using `"META": {"sizes": {...}}` if generated.

To add a new `.grd` entry, first find the location to add it:
* Find the matching section (`chrome/app/`, `chrome/browser/`, etc.).
* Preserve alphabetical order within a section.

##### Simple case: Sufficient most of the time

This applies if the new entry is not at the beginning or end of sections, and
nearby `.grd` entries have distinct-looking fake start IDs without
`"META": {"join": 2}` oddity. For example, adding `foo/bar/ccc.grd` here:
```
  "foo/bar/aaa.grd": {
    "includes": [1140],
    "structures": [1160],
  },
  "foo/bar/bbb.grd": {
    "includes": [1180],
  },
  "foo/bar/ddd.grd": {
    "includes": [1200],
  },
```
The best location is between `bbb.grd` and `ddd.grd`. Each resource type (say,
`includes` and `messages`) must have a sub-entry with fake start IDs. The values
select should be intermediate to neighboring ones, and sorted, e.g.:
```
  "foo/bar/aaa.grd": {
    "includes": [1140],
    "structures": [1160],
  },
  "foo/bar/bbb.grd": {
    "includes": [1180],
  },
  "foo/bar/ccc.grd": {  # NEW [comment for illustration only]
    "includes": [1185],
    "messages": [1190],
  },
  "foo/bar/ddd.grd": {
    "includes": [1200],
  },
```
Done!

##### Special case: Crowded fake start IDs

If fake start IDs get too crowded, then (from this directory) run
```
python3 ../grit/grit.py update_resource_ids -i resource_ids.spec --fake > temp
mv temp resource_ids.spec
```
before (and/or after) your edit to make room. After this, your CLs will have
many diffs in fake start IDs, but that's okay since relative orderings are
preserved.

##### Special case: Generated .grd files

If your .grd file is generated, then in general it's unavailable (e.g., due to
build config) for ID assignment to detect how many IDs to reserve. You will need
to provide a bound on resources for each resource type
({`includes`, `messages`, `structures`}) to reserve, specified using
`"META": {"sizes": {}}`. For example, in
```
  "foo/bar/aaa.grd": {
    "includes": [1140],
    "structures": [1160],
  },
  "foo/bar/bbb.grd": {
    "includes": [1180],
  },
  "<(SHARED_INTERMEDIATE_DIR)/foo/bar/ccc.grd": {  # NEW
    "META": {"sizes": {"includes": [20], "messages": [50],}},
    "includes": [1185],
    "messages": [1190],
  },
  "foo/bar/ddd.grd": {
    "includes": [1200],
  },
```
we reserved 20 IDs for `<include>` resources, and 50 for `<message>` resources.
Hopefully you'll only need to do this once, but if in the future the generated
resources exceeds the bound, then you'll need to update `resource_ids.spec`.

##### Special case: Add at end of split

Suppose you desired location is preceded by repeated values ("splits") and
followed by `"META": {"join": ##}`, e.g.:
```
  "foo/bar/bbb.grd": {
    "includes": [1160],
  },
  "foo/bar/bbb_helper.grd": {
    "includes": [1180],
  },
  "foo/bar/bbb_testing.grd": {
    "includes": [1160],
  },
  "foo/bar/bbb_testing_helper.grd": {
    "includes": [1180],
  },
  "foo/bar/ddd.grd": {
    "META": {"join": 2}
    "includes": [1120],
  },
```
This means:
* {`bbb.grd`, `bbb_helper.grd`} is one group.
* {`bbb_testing.grd`, `bbb_testing_helper.grd`} is another group.
* The two groups are mutually exclusive, and IDs can be reused.
* `ddd.grd` is used by everyone; it "joins" the mutually exclusive branches.

If you're adding `foo/bar/ccc.grd`, and it's unrelated to `bbb*.grd` then the
required update would be:
```
  "foo/bar/bbb.grd": {
    "includes": [1160],
  },
  "foo/bar/bbb_helper.grd": {
    "includes": [1180],
  },
  "foo/bar/bbb_testing.grd": {
    "includes": [1160],
  },
  "foo/bar/bbb_testing_helper.grd": {
    "includes": [1180],
  },
  "foo/bar/ccc.grd": {  # NEW
    "META": {"join": 2}
    "includes": [1190],
  },
  "foo/bar/ddd.grd": {  # "META": {"join": 2} got moved.
    "includes": [1200],
  },
```

If you're adding a new entry to the start or end of sections, pay attention to
potential splits across sections. In particular, note that `chrome/` and
`ios/chrome/` sections are very long splits that finally get joined by the
"everything else" section!

##### Special case: Add new alternative to existing split

Using the same example as above: If you're adding a new alternative, say,
`foo/bar/bbb_demo.grd` then the change should be:
```
  "foo/bar/bbb.grd": {
    "includes": [1160],
  },
  "foo/bar/bbb_helper.grd": {
    "includes": [1180],
  },
  "foo/bar/bbb_demo.grd": {  # NEW, with 1160 to specify alternative.
    "includes": [1160],
  },
  "foo/bar/bbb_testing.grd": {
    "includes": [1160],
  },
  "foo/bar/bbb_testing_helper.grd": {
    "includes": [1180],
  },
  "foo/bar/ddd.grd": {
    "META": {"join": 3}  # 2 became 3.
    "includes": [1200],
  },
```

##### Special case: Creating split

Starting from the simple case again:
```
  "foo/bar/aaa.grd": {
    "includes": [1140],
    "structures": [1160],
  },
  "foo/bar/bbb.grd": {
    "includes": [1180],
  },
  "foo/bar/ddd.grd": {
    "includes": [1200],
  },
```
If you want to add mutually-exclusive `foo/bar/ccc.grd` and
`foo/bar/ccc_testing.grd`, then you'd create a split by repeating a fake start
ID, then update the succeeding entry to join these splits:
```
  "foo/bar/aaa.grd": {
    "includes": [1140],
    "structures": [1160],
  },
  "foo/bar/bbb.grd": {
    "includes": [1180],
  },
  "foo/bar/ccc.grd": {  # NEW
    "includes": [1190],
  },
  "foo/bar/ccc_testing.grd": {  # NEW, reusing number to indicate split.
    "includes": [1190],
  },
  "foo/bar/ddd.grd": {
    "META": {"join": 2},  # Add join.
    "includes": [1200],
  },
```

#### Details on resource_ids.spec

For simplicity, let's disregard that `.grd` files can have different types of
resources ({`includes`, `messages`, `structures`}). Henceforth, each `x.grd`
specifies `count(x.grd)` to specify the number of globally distinct resource IDs
(integers from 0 to 32767) it needs.

Automatic resource ID assignment computes for each `x.grd`, a start ID
`start(x.grd)`, which indicates that resource IDs
`start(x.grd), start(x.grd) + 1, ..., start(x.grd) + count(x.grd) - 1` are
reserved for `x.grd`.

Complication arise from the following:
* Start IDs among `.grd` files must follow a preassigned order for
  determinism (i.e., no "first-come, first-serve" assignment).
* Generated `.grd` files' `count()` are in general not known _a priori_. By
  contrast, static `.grd` files in the source can have `count()` determined
  by parsing.
* Depending on build config, some `.grd` files may be excluded. To avoid
  wasteful large gaps in resource ID space, mutually exclusive `.grd` files can
  share resource IDs.

The (former) `resource_ids` file was introduced to coordinate resource ID
assignment. The file lists every (static and generated) `.grd` file across all
build configs, and specifies start IDs for lookup. This allows resource targets
to be built independently.

However, `resource_ids` required frequent curation. Moreover, adding new
resources to a `.grd` entry (even with extra IDs reserved for buffering) can
"bump" start IDs of adjacent entries. This process can cascade downward,
leading to multiple updates. The workflow was tedious and error-prone.

Automatic start ID assignment was later introduced. It utilizes the following:
* For static `.grd` files, `count()` can be easily parsed.
* For generated `.grd` files, a bound on `count()` can be specified as extra
  data, stored in new field (`"META": {"size": {...}}`).
* The order of elements in `resource_ids` along with preassigned start IDs
  encode some implicit "ordering structure" that specifies:
  * Ordering of start IDs among `.grd` files.
  * Mutual exclusion among groups of `.grd` files.

`resource_ids` is still used, but it's now generated from `resource_id.spec`,
which is a new "specification" file that's largely similar to `resource_ids`,
featuring the following:
* `.grd` entries and order are kept.
* `{"META": {"size": {...}}` is used to specify size bounds of generated `.grd`
  files.
* Start IDs are replaced with "fake start IDs" to capture "ordering structure".

It remains to clarify what "ordering structure" is, and how to represent it with
fake start IDs.

Suppose that all `.grd` files are used, then start ID generation is
straightforward: Visit `.grd` files sequentially, then evaluate `start()` as
cumulative sums of `count()`. We call this a "chain", which conceptually
resembles some "series" connection looking like `A <-- B <-- C`.

In reality, multiple chains exist and `.grd` files in different chains can be
mutually exclusive, allowing ID reuse. Conceptually these resemble "parallel"
connections looking like:
```
A <-- B1 <-- C1 <-- D
|                   |
+ <-- B2 <-- C2 <-- +
```
Here, `A` is a **split** and `D` is a **join**. The vertical bars `|` and `+`
merely stretches vertices downward.

So we're getting into graph theory territory. It turns out that a suitable
representation is subtle. To avoid redundant and confusing artifacts, we need
to be more precise:
* Each `X.grd` file is represented by a **directed edge** weighed by
  `count(X.grd)`, embodying a constraint (more on this later).
* Vertices are variables representing potential `start()` ID values, possibly
  shared among multiple `.grd` files owing to mutual exclusivity.
* Define sentinel vertex `S` as the minimal resource ID available. Abstractly
  We can see it as `0`, though in practice `400` is used.
* Define sentinel vertex 'T = 32768' as the (invalid) terminal resource ID.

Consider the following graph generation rules:
1. Start from `S <-- T`. This depicts a `S.grd` file with `start(S.grd) = S`,
   and that is (trivially) constrained by `start(S.grd) + count(S.grd) <= T`.
2. **Series** op: Transform `A <-- C` into `A <-- B <-- C`. Meaning: Initially
   `A.grd` (old edge) has `start(A.grd) = A` assigned. `C` can be `T`, or the
   start ID of some other `.grd` file(s). IDs are constrained by
   `A + count(A.grd) <= C`. The transformation adds `B.grd` (new edge) with
   start ID `start(B.grd) = B`. The old constraint (edge) is replaced by 2 new
   constraints `A + count(A.grd) <= B` and `B + count(B.grd) <= C`.
3. **Parallel** op: Transform `A <-- B` into `A <== B`, i.e., create a double
   edge. Meaning: Initially `A1.grd` (old edge) has `start(A1.grd) = A`
   assigned. `B` can be `T`, or the start ID of some other `.grd` file(s). IDs
   are constrained by `A + count(A1.grd) <= B`. The transformation adds
   `A2.grd` (new edge) that's mutually exclusive with `A1.grd`, and can
   therefore share the same start value `A`, i.e., `start(A2.grd) = A`. The old
   constraint remains, and we're adding a new one `A + count(A2.grd) <= B`.
   The combined effect of the constraints is
   `A + max(count(A1.grd), count(A2.grd)) <= B`.

Composing rule 1 with arbitrary copies of rule {2, 3] (each targeting an
existing edge, while adding a new one from a new `.grd` file) results in a
[series-parallel graph] with one source `T`, one sink `S`, and no cycles. This
graph exactly represents "ordering structure". Computationally it represents
assigning each `.grd` file's start ID to some to a variables (vertices),
subject to constraints (edges weighted by `count()`).

Given the above graph, solving the assignment problem is easy (omitted). The
challenge is to represent the series-parallel graph in `resource_ids.spec`.
Some possibilities:
* Explicitly define vertices, then list `.grd` files as edges (each with 2
  vertices): It turns out this has "too much freedom", and can lead to errors
  (e.g., cycles), requiring validation. Also, how do we name vertices?
* Use bracketing: Series op is associative; parallel op is associative and
  commutative (over isomorphism). Therefore we can represent series op with a
  list (order important) and parallel op as a set (order unimportant). The graph
  becomes an alternating-nested list and set (noting that list of lists and
  set of sets can be flattened)! For example, `[a,{b,[c,{d,e},f]},{g,h,i},j]`
  would represent (using # for vertices):
```
S <-(a)- # <----------(b)---------- # <-(g)- # <-(j)- T
         |                          |        |
         + <-(c)- # <-(d)- # <-(f)- + <-(h)- +
                  |        |        |        |
                  + <-(e)- +        + <-(i)- +
```
  We might do this in the future -- but this would be a drastic change.
* Use fake start IDs: This is the choice taken, since it mostly maintains the
  old "API" of updating numbers in a file.

To encode a series-parallel graph as a sequence of fake start IDs, first we note
that chains formed by repeated series ops can be represented by increasing fake
start IDs. For example: [0, 1, 2, 3, 4] matches:
```
S <-0-- # <-1-- # <-2-- # <-3-- # <-4-- T
```
Note: The numbers are not to be confused with `count()` as edge weights.

Next, repeated or decreasing fake start IDs can be thought of "rewinding" to
the past, which create splits (like time travel) that parallel ops can use.
This can also be done in a hierarchical fashion. For example,
`[1, 2, 4, 2, 3, 4, 5, 6, 4, 7, 7, 2, 3, 4, 3]` can create
```
S <-1-- # <-2-- # <-4--
        |
        + <-2-- # <-3-- # <-4-- # <-5-- # <-6--
        |               |
        |               + <-4-- # <-7--
        |                       |
        |                       + <-7--
        |
        + <-2-- # <-3-- # <-4--
                |
                # <-3--
```
The tree building algorithm uses a stack: Increasing values are pushed; repeated
or decreasing values cause pops until a match is found, to which linkage can be
applied.

Therefore fake start IDs can represent a tree. However, parallel op also
requires "joins". To do this, we'll need to add "join" to literally "tie up
loose ends". Let's consider a smaller example: `[1, 2, 2, 3, 4]`. The
resulting tree is:
```
S <-1-- # <-2--
        |
        + <-2-- # <-3-- # <-4--
```
If "join" is placed at `3`, we'd get `[1, 2, 2, 3*, 4]`, which represents:
```
S <-1-- # <-2-- # <-3-- # <-4-- T
        |       |
        + <-2-- +
```
If "join" is placed at `4`, we'd get `[1, 2, 2, 3, 4*]`, which represents:
```
S <-1-- # <-----2------ # <-4-- T
        |               |
        + <-2-- # <-3-- +
```
Note that the end sentinel `T` can be thought to absorb all unjoined branches.
Therefore the original tree `[1, 2, 2, 3, 4]` actually represents:
```
S <-1-- # <---------2---------- T
        |                       |
        + <-2-- # <-3-- # <-4-- +
```
Next, joins have a multiplicity, so since more than 2 branches can be joined at
the same node. For example, `[1, 2, 2, 3, 3, 4*, 5*, 5, 5, 6**]` (with `*`
representing a join) results in:
```
S <-1-- # <---------2---------- # <-5-- # <-6-- T
        |                       |       |
        + <-2-- # <-3-- # <-4-- + <-5-- +
                |       |       |       |
                + <-3-- +       + <-5-- +
```
In fact, this is just an earlier example we've seen.

Finally, note that the running count of joints, i.e., `*`, must be less than or
equal to the running count of splits.

[series-parallel graph]: https://en.wikipedia.org/wiki/Series%E2%80%93parallel_graph
