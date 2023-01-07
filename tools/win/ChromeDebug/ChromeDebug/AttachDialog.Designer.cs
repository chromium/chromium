// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace ChromeDebug
{
    partial class AttachDialog
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
      this.listViewProcesses = new System.Windows.Forms.ListView();
      this.columnHeaderProcess = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
      this.columnHeaderPid = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
      this.columnHeaderTitle = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
      this.columnHeaderType = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
      this.columnHeaderSession = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
      this.columnHeaderCmdLine = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
      this.buttonAttach = new System.Windows.Forms.Button();
      this.buttonCancel = new System.Windows.Forms.Button();
      this.groupBox1 = new System.Windows.Forms.GroupBox();
      this.buttonRefresh = new System.Windows.Forms.Button();
      this.checkBoxOnlyChrome = new System.Windows.Forms.CheckBox();
      this.groupBox1.SuspendLayout();
      this.SuspendLayout();
      // 
      // listViewProcesses
      // 
      this.listViewProcesses.AllowColumnReorder = true;
      this.listViewProcesses.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
      this.listViewProcesses.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeaderProcess,
            this.columnHeaderPid,
            this.columnHeaderTitle,
            this.columnHeaderType,
            this.columnHeaderSession,
            this.columnHeaderCmdLine});
      this.listViewProcesses.FullRowSelect = true;
      this.listViewProcesses.Location = new System.Drawing.Point(14, 27);
      this.listViewProcesses.Name = "listViewProcesses";
      this.listViewProcesses.Size = new System.Drawing.Size(884, 462);
      this.listViewProcesses.TabIndex = 0;
      this.listViewProcesses.UseCompatibleStateImageBehavior = false;
      this.listViewProcesses.View = System.Windows.Forms.View.Details;
      // 
      // columnHeaderProcess
      // 
      this.columnHeaderProcess.Text = "Executable";
      this.columnHeaderProcess.Width = 65;
      // 
      // columnHeaderPid
      // 
      this.columnHeaderPid.Text = "PID";
      this.columnHeaderPid.Width = 30;
      // 
      // columnHeaderTitle
      // 
      this.columnHeaderTitle.Text = "Title";
      this.columnHeaderTitle.Width = 32;
      // 
      // columnHeaderType
      // 
      this.columnHeaderType.Text = "Type";
      this.columnHeaderType.Width = 36;
      // 
      // columnHeaderSession
      // 
      this.columnHeaderSession.Text = "Session";
      this.columnHeaderSession.Width = 49;
      // 
      // columnHeaderCmdLine
      // 
      this.columnHeaderCmdLine.Text = "Command Line";
      this.columnHeaderCmdLine.Width = 668;
      // 
      // buttonAttach
      // 
      this.buttonAttach.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
      this.buttonAttach.DialogResult = System.Windows.Forms.DialogResult.OK;
      this.buttonAttach.Location = new System.Drawing.Point(684, 603);
      this.buttonAttach.Name = "buttonAttach";
      this.buttonAttach.Size = new System.Drawing.Size(118, 41);
      this.buttonAttach.TabIndex = 2;
      this.buttonAttach.Text = "Attach";
      this.buttonAttach.UseVisualStyleBackColor = true;
      this.buttonAttach.Click += new System.EventHandler(this.buttonAttach_Click);
      // 
      // buttonCancel
      // 
      this.buttonCancel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
      this.buttonCancel.DialogResult = System.Windows.Forms.DialogResult.Cancel;
      this.buttonCancel.Location = new System.Drawing.Point(808, 603);
      this.buttonCancel.Name = "buttonCancel";
      this.buttonCancel.Size = new System.Drawing.Size(118, 41);
      this.buttonCancel.TabIndex = 3;
      this.buttonCancel.Text = "Cancel";
      this.buttonCancel.UseVisualStyleBackColor = true;
      // 
      // groupBox1
      // 
      this.groupBox1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
      this.groupBox1.Controls.Add(this.listViewProcesses);
      this.groupBox1.Location = new System.Drawing.Point(12, 27);
      this.groupBox1.Name = "groupBox1";
      this.groupBox1.Size = new System.Drawing.Size(914, 511);
      this.groupBox1.TabIndex = 5;
      this.groupBox1.TabStop = false;
      this.groupBox1.Text = "Available Processes";
      // 
      // buttonRefresh
      // 
      this.buttonRefresh.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
      this.buttonRefresh.Location = new System.Drawing.Point(808, 552);
      this.buttonRefresh.Name = "buttonRefresh";
      this.buttonRefresh.Size = new System.Drawing.Size(117, 33);
      this.buttonRefresh.TabIndex = 6;
      this.buttonRefresh.Text = "Refresh";
      this.buttonRefresh.UseVisualStyleBackColor = true;
      this.buttonRefresh.Click += new System.EventHandler(this.buttonRefresh_Click);
      // 
      // checkBoxOnlyChrome
      // 
      this.checkBoxOnlyChrome.AutoSize = true;
      this.checkBoxOnlyChrome.Checked = true;
      this.checkBoxOnlyChrome.CheckState = System.Windows.Forms.CheckState.Checked;
      this.checkBoxOnlyChrome.Location = new System.Drawing.Point(12, 561);
      this.checkBoxOnlyChrome.Name = "checkBoxOnlyChrome";
      this.checkBoxOnlyChrome.Size = new System.Drawing.Size(165, 17);
      this.checkBoxOnlyChrome.TabIndex = 7;
      this.checkBoxOnlyChrome.Text = "Only show Chrome processes";
      this.checkBoxOnlyChrome.UseVisualStyleBackColor = true;
      this.checkBoxOnlyChrome.CheckedChanged += new System.EventHandler(this.checkBoxOnlyChrome_CheckedChanged);
      // 
      // AttachDialog
      // 
      this.AcceptButton = this.buttonAttach;
      this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
      this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
      this.CancelButton = this.buttonCancel;
      this.ClientSize = new System.Drawing.Size(940, 656);
      this.ControlBox = false;
      this.Controls.Add(this.checkBoxOnlyChrome);
      this.Controls.Add(this.buttonRefresh);
      this.Controls.Add(this.groupBox1);
      this.Controls.Add(this.buttonCancel);
      this.Controls.Add(this.buttonAttach);
      this.MaximizeBox = false;
      this.MinimizeBox = false;
      this.Name = "AttachDialog";
      this.ShowInTaskbar = false;
      this.Text = "Attach to Chrome";
      this.Load += new System.EventHandler(this.AttachDialog_Load);
      this.groupBox1.ResumeLayout(false);
      this.ResumeLayout(false);
      this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.ListView listViewProcesses;
        private System.Windows.Forms.Button buttonAttach;
        private System.Windows.Forms.Button buttonCancel;
        private System.Windows.Forms.ColumnHeader columnHeaderProcess;
        private System.Windows.Forms.ColumnHeader columnHeaderPid;
        private System.Windows.Forms.ColumnHeader columnHeaderTitle;
        private System.Windows.Forms.ColumnHeader columnHeaderCmdLine;
        private System.Windows.Forms.ColumnHeader columnHeaderType;
        private System.Windows.Forms.ColumnHeader columnHeaderSession;
        private System.Windows.Forms.GroupBox groupBox1;
        private System.Windows.Forms.Button buttonRefresh;
        private System.Windows.Forms.CheckBox checkBoxOnlyChrome;
    }
}