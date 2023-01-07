// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

using System;
using System.Diagnostics;
using System.Globalization;
using System.Runtime.InteropServices;
using System.ComponentModel.Design;
using Microsoft.Win32;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio.OLE.Interop;
using Microsoft.VisualStudio.Shell;
using System.Windows.Forms;

namespace ChromeDebug {
  /// <summary>
  /// This is the class that implements the package exposed by this assembly.
  ///
  /// The minimum requirement for a class to be considered a valid package for Visual Studio
  /// is to implement the IVsPackage interface and register itself with the shell.
  /// This package uses the helper classes defined inside the Managed Package Framework (MPF)
  /// to do it: it derives from the Package class that provides the implementation of the
  /// IVsPackage interface and uses the registration attributes defined in the framework to
  /// register itself and its components with the shell.
  /// </summary>
  // This attribute tells the PkgDef creation utility (CreatePkgDef.exe) that this class is
  // a package.
  [PackageRegistration(UseManagedResourcesOnly = true)]
  // This attribute is used to register the information needed to show this package
  // in the Help/About dialog of Visual Studio.
  [InstalledProductRegistration("#110", "#112", "1.0", IconResourceID = 400)]
  // This attribute is needed to let the shell know that this package exposes some menus.
  [ProvideMenuResource("Menus.ctmenu", 1)]
  [Guid(GuidList.guidChromeDebugPkgString)]
  public sealed class ChromeDebugPackage : Package {
    /// <summary>
    /// Default constructor of the package.
    /// Inside this method you can place any initialization code that does not require
    /// any Visual Studio service because at this point the package object is created but
    /// not sited yet inside Visual Studio environment. The place to do all the other
    /// initialization is the Initialize method.
    /// </summary>
    public ChromeDebugPackage() {
      Debug.WriteLine(string.Format(CultureInfo.CurrentCulture, "Entering constructor for: {0}",
          this.ToString()));
    }



    /////////////////////////////////////////////////////////////////////////////
    // Overridden Package Implementation
    #region Package Members

    /// <summary>
    /// Initialization of the package; this method is called right after the package is sited, so this is the place
    /// where you can put all the initialization code that rely on services provided by VisualStudio.
    /// </summary>
    protected override void Initialize() {
      Debug.WriteLine(string.Format(CultureInfo.CurrentCulture, "Entering Initialize() of: {0}", this.ToString()));
      base.Initialize();

      // Add our command handlers for menu (commands must exist in the .vsct file)
      OleMenuCommandService mcs = GetService(typeof(IMenuCommandService)) as OleMenuCommandService;
      if (null != mcs) {
        // Create the command for the menu item.
        CommandID menuCommandID = new CommandID(GuidList.guidChromeDebugCmdSet, (int)PkgCmdIDList.cmdidAttachToProcess);
        MenuCommand menuItem = new MenuCommand(MenuItemCallback, menuCommandID);
        mcs.AddCommand(menuItem);
      }
    }
    #endregion

    /// <summary>
    /// This function is the callback used to execute a command when the a menu item is clicked.
    /// See the Initialize method to see how the menu item is associated to this function using
    /// the OleMenuCommandService service and the MenuCommand class.
    /// </summary>
    private void MenuItemCallback(object sender, EventArgs e) {
      // Show a Message Box to prove we were here
      EnvDTE.DTE dte = (EnvDTE.DTE)GetService(typeof(EnvDTE.DTE));

      IVsUIShell uiShell = (IVsUIShell)GetService(typeof(SVsUIShell));
      Guid clsid = Guid.Empty;
      IntPtr parentHwnd = IntPtr.Zero;
      uiShell.GetDialogOwnerHwnd(out parentHwnd);

      NativeWindow parentShim = new NativeWindow();
      parentShim.AssignHandle(parentHwnd);
      AttachDialog dialog = new AttachDialog();
      DialogResult result = dialog.ShowDialog(parentShim);
      if (result == DialogResult.OK) {
        foreach (int selected_id in dialog.SelectedItems) {
          foreach (EnvDTE90a.Process4 p in dte.Debugger.LocalProcesses) {
            System.Diagnostics.Debug.WriteLine("Found process {0}", p.ProcessID);
            if (p.ProcessID != selected_id)
              continue;
            p.Attach();
            System.Diagnostics.Debug.WriteLine("Attaching to process successful.");
            break;
          }
        }
      }
    }
  }
}
