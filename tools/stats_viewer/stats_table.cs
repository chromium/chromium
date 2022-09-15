// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;

namespace StatsViewer
{
  /// <summary>
  /// The stats table shared memory segment contains this
  /// header structure.
  /// </summary>
  [StructLayout(LayoutKind.Sequential)]
  internal struct StatsFileHeader {
    public int version;
    public int size;
    public int max_counters;
    public int max_threads;
  };

  /// <summary>
  /// An entry in the StatsTable.
  /// </summary>
  class StatsTableEntry {
    public StatsTableEntry(int id, string name, StatsTable table) {
      id_ = id;
      name_ = name;
      table_ = table;
    }

    /// <summary>
    /// The unique id for this entry
    /// </summary>
    public int id { get { return id_; } }

    /// <summary>
    /// The name for this entry.
    /// </summary>
    public string name { get { return name_; } }

    /// <summary>
    /// The value of this entry now.  
    /// </summary>
    public int GetValue(int filter_pid) {
      return table_.GetValue(id_, filter_pid);
    }

    private int id_;
    private string name_;
    private StatsTable table_;
  }

  // An interface for StatsCounters
  interface IStatsCounter {
    // The name of the counter
    string name { get; }
  }

  // A counter.
  class StatsCounter : IStatsCounter {
    public StatsCounter(StatsTableEntry entry) {
      entry_ = entry;
    }

    public string name { 
      get {
        return entry_.name; 
      } 
    }

    public int GetValue(int filter_pid) {
      return entry_.GetValue(filter_pid);
    }

    private StatsTableEntry entry_;
  }

  // A timer.
  class StatsTimer : IStatsCounter {
    public StatsTimer(StatsTableEntry entry)
    {
      entry_ = entry;
    }

    public string name { 
      get { 
        return entry_.name; 
      } 
    }

    public int GetValue(int filter_pid) {
      return entry_.GetValue(filter_pid);
    }

    private StatsTableEntry entry_;
  }

  // A rate.
  class StatsCounterRate : IStatsCounter
  {
    public StatsCounterRate(StatsCounter counter, StatsTimer timer) {
      counter_ = counter;
      timer_ = timer;
    }

    public string name { get { return counter_.name; } }

    public int GetCount(int filter_pid) { 
      return counter_.GetValue(filter_pid);
    }

    public int GetTime(int filter_pid) {
      return timer_.GetValue(filter_pid);
    }

    private StatsCounter counter_;
    private StatsTimer timer_;
  }

  /// <summary>
  /// This is a C# reader for the chrome stats_table.
  /// </summary>
  class StatsTable {
    internal const int kMaxThreadNameLength = 32;
    internal const int kMaxCounterNameLength = 32;

    /// <summary>
    /// Open a StatsTable
    /// </summary>
    public StatsTable() {
    }

    #region Public Properties
    /// <summary>
    /// Get access to the counters in the table.
    /// </summary>
    public StatsTableCounters Counters() {
      return new StatsTableCounters(this);
    }

    /// <summary>
    /// Get access to the processes in the table
    /// </summary>
    public ICollection Processes {
      get {
        return new StatsTableProcesses(this);
      }
    }
    #endregion

    #region Internal Properties
    // 
    // The internal methods are accessible to the enumerators
    // and helper classes below.
    //
    
    /// <summary>
    /// Access to the table header
    /// </summary>
    internal StatsFileHeader Header {
      get { return header_; }
    }

    /// <summary>
    /// Get the offset of the ThreadName table
    /// </summary>
    internal long ThreadNamesOffset {
      get {
        return memory_.ToInt64() + Marshal.SizeOf(typeof(StatsFileHeader));
      }
    }

    /// <summary>
    /// Get the offset of the PIDs table
    /// </summary>
    internal long PidsOffset {
      get {
        long offset = ThreadNamesOffset;
        // Thread names table
        offset += AlignedSize(header_.max_threads * kMaxThreadNameLength * 2);
        // Thread TID table
        offset += AlignedSize(header_.max_threads * 
          Marshal.SizeOf(typeof(int)));
        return offset;
      }
    }

    /// <summary>
    /// Get the offset of the CounterName table
    /// </summary>
    internal long CounterNamesOffset {
      get {
        long offset = PidsOffset;
        // Thread PID table
        offset += AlignedSize(header_.max_threads * 
          Marshal.SizeOf(typeof(int)));
        return offset;
      }
    }

    /// <summary>
    /// Get the offset of the Data table
    /// </summary>
    internal long DataOffset {
      get {
        long offset = CounterNamesOffset;
        // Counter names table
        offset += AlignedSize(header_.max_counters * 
          kMaxCounterNameLength * 2);
        return offset;
      }
    }
    #endregion

    #region Public Methods
    /// <summary>
    /// Opens the memory map
    /// </summary>
    /// <returns></returns>
    /// <param name="name">The name of the file to open</param>
    public bool Open(string name) {
      map_handle_ = 
        Win32.OpenFileMapping((int)Win32.MapAccess.FILE_MAP_WRITE, false, 
                              name);
      if (map_handle_ == IntPtr.Zero)
        return false;

      memory_ = 
        Win32.MapViewOfFile(map_handle_, (int)Win32.MapAccess.FILE_MAP_WRITE, 
                            0,0, 0);
      if (memory_ == IntPtr.Zero) {
        Win32.CloseHandle(map_handle_);
        return false;
      }

      header_ = (StatsFileHeader)Marshal.PtrToStructure(memory_, header_.GetType());
      return true;
    }

    /// <summary>
    /// Close the mapped file.
    /// </summary>
    public void Close() {
      Win32.UnmapViewOfFile(memory_);
      Win32.CloseHandle(map_handle_);
    }

    /// <summary>
    /// Zero out the stats file.
    /// </summary>
    public void Zero() {
      long offset = DataOffset;
      for (int threads = 0; threads < header_.max_threads; threads++) {
        for (int counters = 0; counters < header_.max_counters; counters++) {
          Marshal.WriteInt32((IntPtr) offset, 0);
          offset += Marshal.SizeOf(typeof(int));
        }
      }
    }

    /// <summary>
    /// Get the value for a StatsCounterEntry now.
    /// </summary>
    /// <returns></returns>
    /// <param name="filter_pid">If a specific PID is being queried, filter to this PID.  0 means use all data.</param>
    /// <param name="id">The id of the CounterEntry to get the value for.</param>
    public int GetValue(int id, int filter_pid) {
      long pid_offset = PidsOffset;
      long data_offset = DataOffset;
      data_offset += id * (Header.max_threads * 
        Marshal.SizeOf(typeof(int)));
      int rv = 0;
      for (int cols = 0; cols < Header.max_threads; cols++)
      {
        int pid = Marshal.ReadInt32((IntPtr)pid_offset);
        if (filter_pid == 0 || filter_pid == pid)
        {
          rv += Marshal.ReadInt32((IntPtr)data_offset);
        }
        data_offset += Marshal.SizeOf(typeof(int));
        pid_offset += Marshal.SizeOf(typeof(int));
      }
      return rv;
    }
    #endregion

    #region Private Methods
    /// <summary>
    /// Align to 4-byte boundaries
    /// </summary>
    /// <param name="size"></param>
    /// <returns></returns>
    private long AlignedSize(long size) {
      Debug.Assert(sizeof(int) == 4);
      return size + (sizeof(int) - (size % sizeof(int))) % sizeof(int);
    }
    #endregion

    #region Private Members
    private IntPtr memory_;
    private IntPtr map_handle_;
    private StatsFileHeader header_;
    #endregion
  }

  /// <summary>
  /// Enumerable list of Counters in the StatsTable
  /// </summary>
  class StatsTableCounters : ICollection {
    /// <summary>
    /// Create the list of counters
    /// </summary>
    /// <param name="table"></param>
    /// pid</param>
    public StatsTableCounters(StatsTable table) {
      table_ = table;
      counter_hi_water_mark_ = -1;
      counters_ = new List<IStatsCounter>();
      FindCounters();
    }

    /// <summary>
    /// Scans the table for new entries.
    /// </summary>
    public void Update() {
      FindCounters();
    }

    #region IEnumerable Members
    public IEnumerator GetEnumerator() {
      return counters_.GetEnumerator();
    }
    #endregion

    #region ICollection Members
    public void CopyTo(Array array, int index) {
      throw new Exception("The method or operation is not implemented.");
    }

    public int Count {
      get {
        return counters_.Count;
      }
    }

    public bool IsSynchronized {
      get { 
        throw new Exception("The method or operation is not implemented."); 
      }
    }

    public object SyncRoot {
      get { 
        throw new Exception("The method or operation is not implemented."); 
      }
    }
    #endregion

    #region Private Methods
    /// <summary>
    /// Create a counter based on an entry
    /// </summary>
    /// <param name="id"></param>
    /// <param name="name"></param>
    /// <returns></returns>
    private IStatsCounter NameToCounter(int id, string name)
    {
      IStatsCounter rv = null;

      // check if the name has a type encoded
      if (name.Length > 2 && name[1] == ':')
      {
        StatsTableEntry entry = new StatsTableEntry(id, name.Substring(2), table_);
        switch (name[0])
        {
          case 't':
            rv = new StatsTimer(entry);
            break;
          case 'c':
            rv = new StatsCounter(entry);
            break;
        }
      }
      else
      {
        StatsTableEntry entry = new StatsTableEntry(id, name, table_);
        rv = new StatsCounter(entry);
      }

      return rv;
    }

    // If we have two StatsTableEntries with the same name, 
    // attempt to upgrade them to a higher level type.  
    // Example:  A counter + a timer == a rate!
    private void UpgradeCounter(IStatsCounter old_counter, IStatsCounter counter)
    {
      if (old_counter is StatsCounter && counter is StatsTimer)
      {
        StatsCounterRate rate = new StatsCounterRate(old_counter as StatsCounter,
                                          counter as StatsTimer);
        counters_.Remove(old_counter);
        counters_.Add(rate);
      }
      else if (old_counter is StatsTimer && counter is StatsCounter)
      {
        StatsCounterRate rate = new StatsCounterRate(counter as StatsCounter,
                                         old_counter as StatsTimer);
        counters_.Remove(old_counter);
        counters_.Add(rate);
      }
    }

    /// <summary>
    /// Find the counters in the table and insert into the counters_
    /// hash table.
    /// </summary>
    private void FindCounters()
    {
      Debug.Assert(table_.Header.max_counters > 0);

      int index = counter_hi_water_mark_;

      do
      {
        // Find an entry in the table.
        index++;
        long offset = table_.CounterNamesOffset +
          (index * StatsTable.kMaxCounterNameLength * 2);
        string name = Marshal.PtrToStringUni((IntPtr)offset);
        if (name.Length == 0)
          continue;

        // Record that we've already looked at this StatsTableEntry.
        counter_hi_water_mark_ = index;

        IStatsCounter counter = NameToCounter(index, name);

        if (counter != null)
        {
          IStatsCounter old_counter = FindExistingCounter(counter.name);
          if (old_counter != null)
            UpgradeCounter(old_counter, counter);
          else
            counters_.Add(counter);
        }
      } while (index < table_.Header.max_counters - 1);
    }

    /// <summary>
    /// Find an existing counter in our table
    /// </summary>
    /// <param name="name"></param>
    private IStatsCounter FindExistingCounter(string name) {
      foreach (IStatsCounter ctr in counters_)
      {
        if (ctr.name == name)
          return ctr;
      }
      return null;
    }
    #endregion

    #region Private Members
    private StatsTable table_;
    private List<IStatsCounter> counters_;
    // Highest index of counters processed.
    private int counter_hi_water_mark_;
    #endregion
  }

  /// <summary>
  /// A collection of processes
  /// </summary>
  class StatsTableProcesses : ICollection
  {
    /// <summary>
    /// Constructor
    /// </summary>
    /// <param name="table"></param>
    public StatsTableProcesses(StatsTable table) {
      table_ = table;
      pids_ = new List<int>();
      Initialize();
    }

    #region ICollection Members
    public void CopyTo(Array array, int index) {
      throw new Exception("The method or operation is not implemented.");
    }

    public int Count {
      get {
        return pids_.Count;
      }
    }

    public bool IsSynchronized {
      get {
        throw new Exception("The method or operation is not implemented."); 
      }
    }

    public object SyncRoot {
      get { 
        throw new Exception("The method or operation is not implemented."); 
      }
    }
    #endregion

    #region IEnumerable Members
    public IEnumerator GetEnumerator() {
      return pids_.GetEnumerator();
    }
    #endregion

    /// <summary>
    /// Initialize the pid list.
    /// </summary>
    private void Initialize() {
      long offset = table_.ThreadNamesOffset;

      for (int index = 0; index < table_.Header.max_threads; index++) {
        string thread_name = Marshal.PtrToStringUni((IntPtr)offset);
        if (thread_name.Length > 0) {
          long pidOffset = table_.PidsOffset + index * 
            Marshal.SizeOf(typeof(int));
          int pid = Marshal.ReadInt32((IntPtr)pidOffset);
          if (!pids_.Contains(pid))
            pids_.Add(pid);
        }
        offset += StatsTable.kMaxThreadNameLength * 2;
      }
    }

    #region Private Members
    private StatsTable table_;
    private List<int> pids_;
    #endregion
  }
}
