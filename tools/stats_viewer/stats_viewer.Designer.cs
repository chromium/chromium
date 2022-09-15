// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace StatsViewer
{
  partial class StatsViewer
  {
    /// <summary>
    /// Required designer variable.
    /// </summary>
    private System.ComponentModel.IContainer components = null;

    /// <summary>
    /// Clean up any resources being used.
    /// </summary>
    /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
    protected override void Dispose(bool disposing)
    {
      if (disposing && (components != null))
      {
        components.Dispose();
      }
      base.Dispose(disposing);
    }

    #region Windows Form Designer generated code

    /// <summary>
    /// Required method for Designer support - do not modify
    /// the contents of this method with the code editor.
    /// </summary>
    private void InitializeComponent()
    {
      System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(StatsViewer));
      this.listViewCounters = new System.Windows.Forms.ListView();
      this.columnHeaderName = new System.Windows.Forms.ColumnHeader();
      this.columnHeaderValue = new System.Windows.Forms.ColumnHeader();
      this.columnHeaderDelta = new System.Windows.Forms.ColumnHeader();
      this.pictureBoxTitle = new System.Windows.Forms.PictureBox();
      this.panelHeader = new System.Windows.Forms.Panel();
      this.labelKills = new System.Windows.Forms.Label();
      this.label1 = new System.Windows.Forms.Label();
      this.labelInterval = new System.Windows.Forms.Label();
      this.comboBoxFilter = new System.Windows.Forms.ComboBox();
      this.panelControls = new System.Windows.Forms.Panel();
      this.buttonExport = new System.Windows.Forms.Button();
      this.buttonZero = new System.Windows.Forms.Button();
      this.comboBoxInterval = new System.Windows.Forms.ComboBox();
      this.labelFilter = new System.Windows.Forms.Label();
      this.saveFileDialogExport = new System.Windows.Forms.SaveFileDialog();
      this.menuStrip1 = new System.Windows.Forms.MenuStrip();
      this.fileToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
      this.openToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
      this.closeToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
      this.quitToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
      this.listViewRates = new System.Windows.Forms.ListView();
      this.columnHeaderRateName = new System.Windows.Forms.ColumnHeader();
      this.columnHeaderRateCount = new System.Windows.Forms.ColumnHeader();
      this.columnHeaderRateTotaltime = new System.Windows.Forms.ColumnHeader();
      this.columnHeaderRateAvgTime = new System.Windows.Forms.ColumnHeader();
      this.splitContainer1 = new System.Windows.Forms.SplitContainer();
      ((System.ComponentModel.ISupportInitialize)(this.pictureBoxTitle)).BeginInit();
      this.panelHeader.SuspendLayout();
      this.panelControls.SuspendLayout();
      this.menuStrip1.SuspendLayout();
      this.splitContainer1.Panel1.SuspendLayout();
      this.splitContainer1.Panel2.SuspendLayout();
      this.splitContainer1.SuspendLayout();
      this.SuspendLayout();
      // 
      // listViewCounters
      // 
      this.listViewCounters.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeaderName,
            this.columnHeaderValue,
            this.columnHeaderDelta});
      this.listViewCounters.Dock = System.Windows.Forms.DockStyle.Fill;
      this.listViewCounters.FullRowSelect = true;
      this.listViewCounters.Location = new System.Drawing.Point(0, 0);
      this.listViewCounters.Name = "listViewCounters";
      this.listViewCounters.Size = new System.Drawing.Size(505, 221);
      this.listViewCounters.Sorting = System.Windows.Forms.SortOrder.Descending;
      this.listViewCounters.TabIndex = 0;
      this.listViewCounters.UseCompatibleStateImageBehavior = false;
      this.listViewCounters.View = System.Windows.Forms.View.Details;
      this.listViewCounters.ColumnClick += new System.Windows.Forms.ColumnClickEventHandler(this.column_Click);
      // 
      // columnHeaderName
      // 
      this.columnHeaderName.Text = "Counter Name";
      this.columnHeaderName.Width = 203;
      // 
      // columnHeaderValue
      // 
      this.columnHeaderValue.Text = "Value";
      this.columnHeaderValue.Width = 69;
      // 
      // columnHeaderDelta
      // 
      this.columnHeaderDelta.Text = "Delta";
      this.columnHeaderDelta.Width = 86;
      // 
      // pictureBoxTitle
      // 
      this.pictureBoxTitle.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
      this.pictureBoxTitle.BackColor = System.Drawing.Color.Transparent;
      this.pictureBoxTitle.Image = ((System.Drawing.Image)(resources.GetObject("pictureBoxTitle.Image")));
      this.pictureBoxTitle.Location = new System.Drawing.Point(257, 0);
      this.pictureBoxTitle.Name = "pictureBoxTitle";
      this.pictureBoxTitle.Size = new System.Drawing.Size(248, 86);
      this.pictureBoxTitle.TabIndex = 1;
      this.pictureBoxTitle.TabStop = false;
      // 
      // panelHeader
      // 
      this.panelHeader.BackgroundImage = ((System.Drawing.Image)(resources.GetObject("panelHeader.BackgroundImage")));
      this.panelHeader.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Stretch;
      this.panelHeader.Controls.Add(this.labelKills);
      this.panelHeader.Controls.Add(this.label1);
      this.panelHeader.Controls.Add(this.pictureBoxTitle);
      this.panelHeader.Dock = System.Windows.Forms.DockStyle.Top;
      this.panelHeader.Location = new System.Drawing.Point(0, 24);
      this.panelHeader.Name = "panelHeader";
      this.panelHeader.Size = new System.Drawing.Size(505, 86);
      this.panelHeader.TabIndex = 2;
      // 
      // labelKills
      // 
      this.labelKills.AutoSize = true;
      this.labelKills.BackColor = System.Drawing.Color.Transparent;
      this.labelKills.Font = new System.Drawing.Font("Arial", 9.75F, System.Drawing.FontStyle.Italic, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
      this.labelKills.Location = new System.Drawing.Point(12, 33);
      this.labelKills.Name = "labelKills";
      this.labelKills.Size = new System.Drawing.Size(280, 16);
      this.labelKills.TabIndex = 3;
      this.labelKills.Text = "During the World Wide Wait, God Kills Kittens.";
      // 
      // label1
      // 
      this.label1.AutoSize = true;
      this.label1.BackColor = System.Drawing.Color.Transparent;
      this.label1.Font = new System.Drawing.Font("Arial", 15.75F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
      this.label1.Location = new System.Drawing.Point(12, 9);
      this.label1.Name = "label1";
      this.label1.Size = new System.Drawing.Size(140, 24);
      this.label1.TabIndex = 2;
      this.label1.Text = "Chrome Varz";
      // 
      // labelInterval
      // 
      this.labelInterval.AutoSize = true;
      this.labelInterval.Location = new System.Drawing.Point(11, 9);
      this.labelInterval.Name = "labelInterval";
      this.labelInterval.Size = new System.Drawing.Size(73, 13);
      this.labelInterval.TabIndex = 3;
      this.labelInterval.Text = "Interval (secs)";
      // 
      // comboBoxFilter
      // 
      this.comboBoxFilter.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
      this.comboBoxFilter.FormattingEnabled = true;
      this.comboBoxFilter.Location = new System.Drawing.Point(302, 5);
      this.comboBoxFilter.Name = "comboBoxFilter";
      this.comboBoxFilter.Size = new System.Drawing.Size(121, 21);
      this.comboBoxFilter.TabIndex = 5;
      this.comboBoxFilter.SelectedIndexChanged += new System.EventHandler(this.filter_changed);
      this.comboBoxFilter.DropDownClosed += new System.EventHandler(this.mouse_Leave);
      this.comboBoxFilter.DropDown += new System.EventHandler(this.mouse_Enter);
      // 
      // panelControls
      // 
      this.panelControls.Controls.Add(this.buttonExport);
      this.panelControls.Controls.Add(this.buttonZero);
      this.panelControls.Controls.Add(this.comboBoxInterval);
      this.panelControls.Controls.Add(this.labelFilter);
      this.panelControls.Controls.Add(this.comboBoxFilter);
      this.panelControls.Controls.Add(this.labelInterval);
      this.panelControls.Dock = System.Windows.Forms.DockStyle.Top;
      this.panelControls.Location = new System.Drawing.Point(0, 110);
      this.panelControls.Name = "panelControls";
      this.panelControls.Size = new System.Drawing.Size(505, 32);
      this.panelControls.TabIndex = 6;
      // 
      // buttonExport
      // 
      this.buttonExport.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
      this.buttonExport.Location = new System.Drawing.Point(187, 4);
      this.buttonExport.Name = "buttonExport";
      this.buttonExport.Size = new System.Drawing.Size(75, 23);
      this.buttonExport.TabIndex = 9;
      this.buttonExport.Text = "Export";
      this.buttonExport.UseVisualStyleBackColor = true;
      this.buttonExport.Click += new System.EventHandler(this.buttonExport_Click);
      // 
      // buttonZero
      // 
      this.buttonZero.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
      this.buttonZero.Location = new System.Drawing.Point(427, 4);
      this.buttonZero.Name = "buttonZero";
      this.buttonZero.Size = new System.Drawing.Size(75, 23);
      this.buttonZero.TabIndex = 8;
      this.buttonZero.Text = "Clear All";
      this.buttonZero.UseVisualStyleBackColor = true;
      this.buttonZero.Click += new System.EventHandler(this.buttonZero_Click);
      // 
      // comboBoxInterval
      // 
      this.comboBoxInterval.FormattingEnabled = true;
      this.comboBoxInterval.Items.AddRange(new object[] {
            "1",
            "2",
            "5",
            "10",
            "30",
            "60"});
      this.comboBoxInterval.Location = new System.Drawing.Point(84, 6);
      this.comboBoxInterval.Name = "comboBoxInterval";
      this.comboBoxInterval.Size = new System.Drawing.Size(55, 21);
      this.comboBoxInterval.TabIndex = 7;
      this.comboBoxInterval.Text = "1";
      this.comboBoxInterval.SelectedIndexChanged += new System.EventHandler(this.interval_changed);
      this.comboBoxInterval.DropDownClosed += new System.EventHandler(this.mouse_Leave);
      this.comboBoxInterval.DropDown += new System.EventHandler(this.mouse_Enter);
      // 
      // labelFilter
      // 
      this.labelFilter.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
      this.labelFilter.AutoSize = true;
      this.labelFilter.Location = new System.Drawing.Point(268, 9);
      this.labelFilter.Name = "labelFilter";
      this.labelFilter.Size = new System.Drawing.Size(29, 13);
      this.labelFilter.TabIndex = 6;
      this.labelFilter.Text = "Filter";
      // 
      // saveFileDialogExport
      // 
      this.saveFileDialogExport.FileName = "results.txt";
      // 
      // menuStrip1
      // 
      this.menuStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.fileToolStripMenuItem});
      this.menuStrip1.Location = new System.Drawing.Point(0, 0);
      this.menuStrip1.Name = "menuStrip1";
      this.menuStrip1.Size = new System.Drawing.Size(505, 24);
      this.menuStrip1.TabIndex = 7;
      this.menuStrip1.Text = "menuStrip1";
      // 
      // fileToolStripMenuItem
      // 
      this.fileToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.openToolStripMenuItem,
            this.closeToolStripMenuItem,
            this.quitToolStripMenuItem});
      this.fileToolStripMenuItem.Name = "fileToolStripMenuItem";
      this.fileToolStripMenuItem.Size = new System.Drawing.Size(35, 20);
      this.fileToolStripMenuItem.Text = "File";
      // 
      // openToolStripMenuItem
      // 
      this.openToolStripMenuItem.Name = "openToolStripMenuItem";
      this.openToolStripMenuItem.Size = new System.Drawing.Size(111, 22);
      this.openToolStripMenuItem.Text = "Open";
      this.openToolStripMenuItem.Click += new System.EventHandler(this.openToolStripMenuItem_Click);
      // 
      // closeToolStripMenuItem
      // 
      this.closeToolStripMenuItem.Name = "closeToolStripMenuItem";
      this.closeToolStripMenuItem.Size = new System.Drawing.Size(111, 22);
      this.closeToolStripMenuItem.Text = "Close";
      this.closeToolStripMenuItem.Click += new System.EventHandler(this.closeToolStripMenuItem_Click);
      // 
      // quitToolStripMenuItem
      // 
      this.quitToolStripMenuItem.Name = "quitToolStripMenuItem";
      this.quitToolStripMenuItem.Size = new System.Drawing.Size(111, 22);
      this.quitToolStripMenuItem.Text = "Quit";
      this.quitToolStripMenuItem.Click += new System.EventHandler(this.quitToolStripMenuItem_Click);
      // 
      // listViewRates
      // 
      this.listViewRates.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeaderRateName,
            this.columnHeaderRateCount,
            this.columnHeaderRateTotaltime,
            this.columnHeaderRateAvgTime});
      this.listViewRates.Dock = System.Windows.Forms.DockStyle.Fill;
      this.listViewRates.FullRowSelect = true;
      this.listViewRates.Location = new System.Drawing.Point(0, 0);
      this.listViewRates.Name = "listViewRates";
      this.listViewRates.Size = new System.Drawing.Size(505, 270);
      this.listViewRates.Sorting = System.Windows.Forms.SortOrder.Descending;
      this.listViewRates.TabIndex = 8;
      this.listViewRates.UseCompatibleStateImageBehavior = false;
      this.listViewRates.View = System.Windows.Forms.View.Details;
      // 
      // columnHeaderRateName
      // 
      this.columnHeaderRateName.Text = "Rate Name";
      this.columnHeaderRateName.Width = 205;
      // 
      // columnHeaderRateCount
      // 
      this.columnHeaderRateCount.Text = "Count";
      // 
      // columnHeaderRateTotaltime
      // 
      this.columnHeaderRateTotaltime.Text = "Total Time (ms)";
      this.columnHeaderRateTotaltime.Width = 100;
      // 
      // columnHeaderRateAvgTime
      // 
      this.columnHeaderRateAvgTime.Text = "Average Time (ms)";
      this.columnHeaderRateAvgTime.Width = 110;
      // 
      // splitContainer1
      // 
      this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
      this.splitContainer1.Location = new System.Drawing.Point(0, 142);
      this.splitContainer1.Name = "splitContainer1";
      this.splitContainer1.Orientation = System.Windows.Forms.Orientation.Horizontal;
      // 
      // splitContainer1.Panel1
      // 
      this.splitContainer1.Panel1.Controls.Add(this.listViewCounters);
      // 
      // splitContainer1.Panel2
      // 
      this.splitContainer1.Panel2.Controls.Add(this.listViewRates);
      this.splitContainer1.Size = new System.Drawing.Size(505, 495);
      this.splitContainer1.SplitterDistance = 221;
      this.splitContainer1.TabIndex = 9;
      // 
      // StatsViewer
      // 
      this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
      this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
      this.ClientSize = new System.Drawing.Size(505, 637);
      this.Controls.Add(this.splitContainer1);
      this.Controls.Add(this.panelControls);
      this.Controls.Add(this.panelHeader);
      this.Controls.Add(this.menuStrip1);
      this.DoubleBuffered = true;
      this.Name = "StatsViewer";
      this.Text = "Chrome Varz";
      ((System.ComponentModel.ISupportInitialize)(this.pictureBoxTitle)).EndInit();
      this.panelHeader.ResumeLayout(false);
      this.panelHeader.PerformLayout();
      this.panelControls.ResumeLayout(false);
      this.panelControls.PerformLayout();
      this.menuStrip1.ResumeLayout(false);
      this.menuStrip1.PerformLayout();
      this.splitContainer1.Panel1.ResumeLayout(false);
      this.splitContainer1.Panel2.ResumeLayout(false);
      this.splitContainer1.ResumeLayout(false);
      this.ResumeLayout(false);
      this.PerformLayout();

    }

    #endregion

    private System.Windows.Forms.ListView listViewCounters;
    private System.Windows.Forms.ColumnHeader columnHeaderName;
    private System.Windows.Forms.ColumnHeader columnHeaderValue;
    private System.Windows.Forms.ColumnHeader columnHeaderDelta;
    private System.Windows.Forms.PictureBox pictureBoxTitle;
    private System.Windows.Forms.Panel panelHeader;
    private System.Windows.Forms.Label label1;
    private System.Windows.Forms.Label labelInterval;
    private System.Windows.Forms.ComboBox comboBoxFilter;
    private System.Windows.Forms.Panel panelControls;
    private System.Windows.Forms.Label labelFilter;
    private System.Windows.Forms.ComboBox comboBoxInterval;
    private System.Windows.Forms.Label labelKills;
    private System.Windows.Forms.Button buttonZero;
    private System.Windows.Forms.Button buttonExport;
    private System.Windows.Forms.SaveFileDialog saveFileDialogExport;
    private System.Windows.Forms.MenuStrip menuStrip1;
    private System.Windows.Forms.ToolStripMenuItem fileToolStripMenuItem;
    private System.Windows.Forms.ToolStripMenuItem openToolStripMenuItem;
    private System.Windows.Forms.ToolStripMenuItem closeToolStripMenuItem;
    private System.Windows.Forms.ToolStripMenuItem quitToolStripMenuItem;
    private System.Windows.Forms.ListView listViewRates;
    private System.Windows.Forms.ColumnHeader columnHeaderRateName;
    private System.Windows.Forms.ColumnHeader columnHeaderRateCount;
    private System.Windows.Forms.ColumnHeader columnHeaderRateTotaltime;
    private System.Windows.Forms.ColumnHeader columnHeaderRateAvgTime;
    private System.Windows.Forms.SplitContainer splitContainer1;
  }
}
