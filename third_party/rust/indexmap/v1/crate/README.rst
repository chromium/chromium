indexmap
========

|build_status|_ |crates|_ |docs|_ |rustc|_

.. |build_status| image:: https://github.com/bluss/indexmap/workflows/Continuous%20integration/badge.svg?branch=master
.. _build_status: https://github.com/bluss/indexmap/actions

.. |crates| image:: https://img.shields.io/crates/v/indexmap.svg
.. _crates: https://crates.io/crates/indexmap

.. |docs| image:: https://docs.rs/indexmap/badge.svg
.. _docs: https://docs.rs/indexmap

.. |rustc| image:: https://img.shields.io/badge/rust-1.49%2B-orange.svg
.. _rustc: https://img.shields.io/badge/rust-1.49%2B-orange.svg

A pure-Rust hash table which preserves (in a limited sense) insertion order.

This crate implements compact map and set data-structures,
where the iteration order of the keys is independent from their hash or
value. It preserves insertion order (except after removals), and it
allows lookup of entries by either hash table key or numerical index.

Note: this crate was originally released under the name ``ordermap``,
but it was renamed to ``indexmap`` to better reflect its features.

Background
==========

This was inspired by Python 3.6's new dict implementation (which remembers
the insertion order and is fast to iterate, and is compact in memory).

Some of those features were translated to Rust, and some were not. The result
was indexmap, a hash table that has following properties:

- Order is **independent of hash function** and hash values of keys.
- Fast to iterate.
- Indexed in compact space.
- Preserves insertion order **as long** as you don't call ``.remove()``.
- Uses hashbrown for the inner table, just like Rust's libstd ``HashMap`` does.

Performance
-----------

``IndexMap`` derives a couple of performance facts directly from how it is constructed,
which is roughly:

  A raw hash table of key-value indices, and a vector of key-value pairs.

- Iteration is very fast since it is on the dense key-values.
- Removal is fast since it moves memory areas only in the table,
  and uses a single swap in the vector.
- Lookup is fast-ish because the initial 7-bit hash lookup uses SIMD, and indices are
  densely stored. Lookup also is slow-ish since the actual key-value pairs are stored
  separately. (Visible when cpu caches size is limiting.)

- In practice, ``IndexMap`` has been tested out as the hashmap in rustc in PR45282_ and
  the performance was roughly on par across the whole workload. 
- If you want the properties of ``IndexMap``, or its strongest performance points
  fits your workload, it might be the best hash table implementation.

.. _PR45282: https://github.com/rust-lang/rust/pull/45282


Recent Changes
==============

See `RELEASES.rst <./RELEASES.rst>`_.
