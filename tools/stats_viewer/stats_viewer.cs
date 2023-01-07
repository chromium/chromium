// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Text;
using System.Windows.Forms;
using System.IO;

namespace StatsViewer {
  public partial class StatsViewer : Form  {
    /// <summary>
    /// Create a StatsViewer.
    /// </summary>
    public StatsViewer() {
      InitializeComponent();
    }

    #region Protected Methods
    /// <summary>
    /// Callback when the form loads.
    /// </summary>
    /// <param name="e"></param>
    protected override void OnLoad(EventArgs e) {
      base.OnLoad(e);

      timer_ = new Timer();
      timer_.Interval = kPollInterval;
      timer_.Tick += new EventHandler(PollTimerTicked);
      timer_.Start();
    }
    #endregion

    #region Private Methods
    /// <summary>
    /// Attempt to open the stats file.
    /// Return true on success, false otherwise.
    /// </summary>
    private bool OpenStatsFile() {
      StatsTable table = new StatsTable();
      if (table.Open(kStatsTableName)) {
        stats_table_ = table;
        return true;
      }
      return false;
    }

    /// <summary>
    /// Close the open stats file.
    /// </summary>
    private void CloseStatsFile() {
      if (this.stats_table_ != null)
      {
        this.stats_table_.Close();
        this.stats_table_ = null;
        this.listViewCounters.Items.Clear();
      }
    }

    /// <summary>
    /// Updates the process list in the UI.
    /// </summary>
    private void UpdateProcessList() {
      int current_pids = comboBoxFilter.Items.Count;
      int table_pids = stats_table_.Processes.Count;
      if (current_pids != table_pids + 1)  // Add one because of the "all" entry.
      {
        int selected_index = this.comboBoxFilter.SelectedIndex;
        this.comboBoxFilter.Items.Clear();
        this.comboBoxFilter.Items.Add(kStringAllProcesses);
        foreach (int pid in stats_table_.Processes)
          this.comboBoxFilter.Items.Add(kStringProcess + pid.ToString());
        this.comboBoxFilter.SelectedIndex = selected_index;
      }
    }

    /// <summary>
    /// Updates the UI for a counter.
    /// </summary>
    /// <param name="counter"></param>
    private void UpdateCounter(IStatsCounter counter) {
      ListView view;

      // Figure out which list this counter goes into.
      if (counter is StatsCounterRate)
        view = listViewRates;
      else if (counter is StatsCounter || counter is StatsTimer)
        view = listViewCounters;
      else
        return; // Counter type not supported yet.

      // See if the counter is already in the list.
      ListViewItem item = view.Items[counter.name];
      if (item != null)
      {
        // Update an existing counter.
        Debug.Assert(item is StatsCounterListViewItem);
        StatsCounterListViewItem counter_item = item as StatsCounterListViewItem;
        counter_item.Update(counter, filter_pid_);
      }
      else
      {
        // Create a new counter
        StatsCounterListViewItem new_item = null;
        if (counter is StatsCounterRate)
          new_item = new RateListViewItem(counter, filter_pid_);
        else if (counter is StatsCounter || counter is StatsTimer)
          new_item = new CounterListViewItem(counter, filter_pid_);
        Debug.Assert(new_item != null);
        view.Items.Add(new_item);
      }
    }

    /// <summary>
    /// Sample the data and update the UI
    /// </summary>
    private void SampleData() {
      // If the table isn't open, try to open it again.
      if (stats_table_ == null)
        if (!OpenStatsFile())
          return;

      if (stats_counters_ == null)
        stats_counters_ = stats_table_.Counters();

      if (pause_updates_)
        return;

      stats_counters_.Update();

      UpdateProcessList();

      foreach (IStatsCounter counter in stats_counters_)
        UpdateCounter(counter);
    }

    /// <summary>
    /// Set the background color based on the value
    /// </summary>
    /// <param name="item"></param>
    /// <param name="value"></param>
    private void ColorItem(ListViewItem item, int value)
    {
      if (value < 0)
        item.ForeColor = Color.Red;
      else if (value > 0)
        item.ForeColor = Color.DarkGreen;
      else
        item.ForeColor = Color.Black;
    }

    /// <summary>
    /// Called when the timer fires.
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    void PollTimerTicked(object sender, EventArgs e) {
      SampleData();
    }

    /// <summary>
    /// Called when the interval is changed by the user.
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    private void interval_changed(object sender, EventArgs e) {
      int interval = 1;
      if (int.TryParse(comboBoxInterval.Text, out interval)) {
        if (timer_ != null) {
          timer_.Stop();
          timer_.Interval = interval * 1000;
          timer_.Start();
        }
      } else {
        comboBoxInterval.Text = timer_.Interval.ToString();
      }
    }

    /// <summary>
    /// Called when the user changes the filter
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    private void filter_changed(object sender, EventArgs e) {
      // While in this event handler, don't allow recursive events!
      this.comboBoxFilter.SelectedIndexChanged -= new System.EventHandler(this.filter_changed);
      if (this.comboBoxFilter.Text == kStringAllProcesses)
        filter_pid_ = 0;
      else
        int.TryParse(comboBoxFilter.Text.Substring(kStringProcess.Length), out filter_pid_);
      SampleData();
      this.comboBoxFilter.SelectedIndexChanged += new System.EventHandler(this.filter_changed);
    }

    /// <summary>
    /// Callback when the mouse enters a control
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    private void mouse_Enter(object sender, EventArgs e) {
      // When the dropdown is expanded, we pause 
      // updates, as it messes with the UI.
      pause_updates_ = true;
    }

    /// <summary>
    /// Callback when the mouse leaves a control
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    private void mouse_Leave(object sender, EventArgs e) {
      pause_updates_ = false;
    }

    /// <summary>
    /// Called when the user clicks the zero-stats button.
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    private void buttonZero_Click(object sender, EventArgs e) {
      this.stats_table_.Zero();
      SampleData();
    }

    /// <summary>
    /// Called when the user clicks a column heading.
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    private void column_Click(object sender, ColumnClickEventArgs e) {
      if (e.Column != sort_column_) {
        sort_column_ = e.Column;
        this.listViewCounters.Sorting = SortOrder.Ascending;
      } else {
        if (this.listViewCounters.Sorting == SortOrder.Ascending)
          this.listViewCounters.Sorting = SortOrder.Descending;
        else
          this.listViewCounters.Sorting = SortOrder.Ascending;
      }

      this.listViewCounters.ListViewItemSorter =
          new ListViewItemComparer(e.Column, this.listViewCounters.Sorting);
      this.listViewCounters.Sort();
    }

    /// <summary>
    /// Called when the user clicks the button "Export".
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="e"></param>
    private void buttonExport_Click(object sender, EventArgs e) {
      //Have to pick a textfile to export to.
      //Saves what is shown in listViewStats in the format: function   value
      //(with a tab in between), so that it is easy to copy paste into a spreadsheet.
      //(Does not save the delta values.)
      TextWriter tw = null;
      try {
        saveFileDialogExport.CheckFileExists = false;
        saveFileDialogExport.ShowDialog();
        tw = new StreamWriter(saveFileDialogExport.FileName);

        for (int i = 0; i < listViewCounters.Items.Count; i++) {
          tw.Write(listViewCounters.Items[i].SubItems[0].Text + "\t");
          tw.WriteLine(listViewCounters.Items[i].SubItems[1].Text);
        }
      }
      catch (IOException ex) {
        MessageBox.Show(string.Format("There was an error while saving your results file. The results might not have been saved correctly.: {0}", ex.Message));
      } 
      finally{
        if (tw != null) tw.Close();
      }
    }

    #endregion

    class ListViewItemComparer : IComparer {
      public ListViewItemComparer() {
        this.col_ = 0;
        this.order_ = SortOrder.Ascending;
      }

      public ListViewItemComparer(int column, SortOrder order) {
        this.col_ = column;
        this.order_ = order;
      }

      public int Compare(object x, object y) {
        int return_value = -1;

        object x_tag = ((ListViewItem)x).SubItems[col_].Tag;
        object y_tag = ((ListViewItem)y).SubItems[col_].Tag;

        if (Comparable(x_tag, y_tag))
          return_value = ((IComparable)x_tag).CompareTo(y_tag);
        else
          return_value = String.Compare(((ListViewItem)x).SubItems[col_].Text,
              ((ListViewItem)y).SubItems[col_].Text);

        if (order_ == SortOrder.Descending)
          return_value *= -1;

        return return_value;
      }

      #region Private Methods
      private bool Comparable(object x, object y) {
        if (x == null || y == null)
          return false;

        return x is IComparable && y is IComparable;
      }
      #endregion

      #region Private Members
      private int col_;
      private SortOrder order_;
      #endregion
    }

    #region Private Members
    private const string kStringAllProcesses = "All Processes";
    private const string kStringProcess = "Process ";
    private const int kPollInterval = 1000;  // 1 second
    private const string kStatsTableName = "ChromeStats";
    private StatsTable stats_table_;
    private StatsTableCounters stats_counters_;
    private Timer timer_;
    private int filter_pid_;
    private bool pause_updates_;
    private int sort_column_ = -1;
    #endregion

    #region Private Event Callbacks
    private void openToolStripMenuItem_Click(object sender, EventArgs e)
    {
      OpenDialog dialog = new OpenDialog();
      dialog.ShowDialog();

      CloseStatsFile();

      StatsTable table = new StatsTable();
      bool rv = table.Open(dialog.FileName);
      if (!rv)
      {
        MessageBox.Show("Could not open statsfile: " + dialog.FileName);
      }
      else
      {
        stats_table_ = table;
      }
    }

    private void closeToolStripMenuItem_Click(object sender, EventArgs e)
    {
      CloseStatsFile();
    }

    private void quitToolStripMenuItem_Click(object sender, EventArgs e)
    {
      Application.Exit();
    }
    #endregion
  }

  /// <summary>
  /// Base class for counter list view items.
  /// </summary>
  internal class StatsCounterListViewItem : ListViewItem
  {
    /// <summary>
    /// Create the ListViewItem
    /// </summary>
    /// <param name="text"></param>
    public StatsCounterListViewItem(string text) : base(text) { }

    /// <summary>
    /// Update the ListViewItem given a new counter value.
    /// </summary>
    /// <param name="counter"></param>
    /// <param name="filter_pid"></param>
    public virtual void Update(IStatsCounter counter, int filter_pid) { }

    /// <summary>
    /// Set the background color based on the value
    /// </summary>
    /// <param name="value"></param>
    protected void ColorItem(int value)
    {
      if (value < 0)
        ForeColor = Color.Red;
      else if (value > 0)
        ForeColor = Color.DarkGreen;
      else
        ForeColor = Color.Black;
    }

    /// <summary>
    /// Create a new subitem with a zeroed Tag.
    /// </summary>
    /// <returns></returns>
    protected ListViewSubItem NewSubItem()
    {
      ListViewSubItem item = new ListViewSubItem();
      item.Tag = -1;  // Arbitrarily initialize to -1.
      return item;
    }

    /// <summary>
    /// Set the value for a subitem.
    /// </summary>
    /// <param name="item"></param>
    /// <param name="val"></param>
    /// <returns>True if the value changed, false otherwise</returns>
    protected bool SetSubItem(ListViewSubItem item, int val)
    {
      // The reason for doing this extra compare is because 
      // we introduce flicker if we unnecessarily update the 
      // subitems.  The UI is much less likely to cause you
      // a seizure when we do this.
      if (val != (int)item.Tag)
      {
        item.Text = val.ToString();
        item.Tag = val;
        return true;
      }
      return false;
    }
  }

  /// <summary>
  /// A listview item which contains a rate.
  /// </summary>
  internal class RateListViewItem : StatsCounterListViewItem
  {
    public RateListViewItem(IStatsCounter ctr, int filter_pid) :
      base(ctr.name)
    {
      StatsCounterRate rate = ctr as StatsCounterRate;
      Name = rate.name;
      SubItems.Add(NewSubItem());
      SubItems.Add(NewSubItem());
      SubItems.Add(NewSubItem());
      Update(ctr, filter_pid);
    }

    public override void Update(IStatsCounter counter, int filter_pid)
    {
      Debug.Assert(counter is StatsCounterRate);

      StatsCounterRate new_rate = counter as StatsCounterRate;
      int new_count = new_rate.GetCount(filter_pid);
      int new_time = new_rate.GetTime(filter_pid);
      int old_avg = Tag != null ? (int)Tag : 0;
      int new_avg = new_count > 0 ? (new_time / new_count) : 0;
      int delta = new_avg - old_avg;

      SetSubItem(SubItems[column_count_index], new_count);
      SetSubItem(SubItems[column_time_index], new_time);
      if (SetSubItem(SubItems[column_avg_index], new_avg)) 
        ColorItem(delta);
      Tag = new_avg;
    }

    private const int column_count_index = 1;
    private const int column_time_index = 2;
    private const int column_avg_index = 3;
  }

  /// <summary>
  /// A listview item which contains a counter.
  /// </summary>
  internal class CounterListViewItem : StatsCounterListViewItem
  {
    public CounterListViewItem(IStatsCounter ctr, int filter_pid) :
      base(ctr.name)
    {
      Name = ctr.name;
      SubItems.Add(NewSubItem());
      SubItems.Add(NewSubItem());
      Update(ctr, filter_pid);
    }

    public override void Update(IStatsCounter counter, int filter_pid) {
      Debug.Assert(counter is StatsCounter || counter is StatsTimer);

      int new_value = 0;
      if (counter is StatsCounter)
        new_value = ((StatsCounter)counter).GetValue(filter_pid);
      else if (counter is StatsTimer)
        new_value = ((StatsTimer)counter).GetValue(filter_pid);

      int old_value = Tag != null ? (int)Tag : 0;
      int delta = new_value - old_value;
      SetSubItem(SubItems[column_value_index], new_value);
      if (SetSubItem(SubItems[column_delta_index], delta))
        ColorItem(delta);
      Tag = new_value;
    }

    private const int column_value_index = 1;
    private const int column_delta_index = 2;
  }
}
