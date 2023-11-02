// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace StatsViewer
{
  public partial class OpenDialog : Form
  {
    public OpenDialog()
    {
      InitializeComponent();
    }

    /// <summary>
    /// Get the user selected filename
    /// </summary>
    public string FileName
    {
      get { 
        return this.name_box_.Text;
      }
    }

    private void button1_Click(object sender, EventArgs e)
    {
      this.Close();
    }

    private void OnKeyUp(object sender, KeyEventArgs e)
    {
      if (e.KeyCode == Keys.Enter)
      {
        this.Close();
      }
    }
  }
}
