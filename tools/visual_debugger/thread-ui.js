// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class represent the UI for Thread drawing filter.
//
class ThreadUI extends HTMLElement {
    constructor() {
      super();
    }

    connectedCallback() {
      this.innerHTML = `
  <style>
  .row {
    display: flex;
    justify-content: space-between;
    padding-bottom: 10px;
  }

  #thread-container {
    max-width: 420px;
    min-width: 350px;
    font-family: Roboto;
    font-size: 10pt;
    background-color: white;
    padding: 10px;
  }

  #saveThreadOptions {
    margin: 20px 10px;
    margin-right: 0px;
  }
  </style>

  <div id='thread-container'>
    <div class='sectionTitle' id='thread-ui-title'>
      Edit Thread Draw Options
    </div>
    <div style='display: flex'>
      <div style='flex-grow: 1'>
        <div class='sectionTitle'>Action</div>
        <div class='section'>
          <div>
            <form id="actionform">
              <input type='radio' id='threadOverride' name='dowhat'
                value='threadOverridesFilters'/>Override filters
              <input type='color' id='threadColor' name='drawcolor'
                value='#000000'/>
              opacity <input type='range' name='fillalpha' min='0' max='100'
               step='10' value='50' list='alphastep'/>
              <datalist id='alphastep'>
                <option>0</option><option>10</option><option>20</option>
                <option>30</option><option>40</option><option>50</option>
                <option>60</option> <option>70</option><option>80</option>
                <option>90</option><option>100</option>
              </datalist>
              <input checked type='radio' name='dowhat'
                value='drawWithFilterOptions'/>
              Draw with filter color and opacity
              <br/>
            </form>
          </div>
        </div>
      </div>
      <button id='saveThreadOptions'>Save</button>
    </div>
  <div>
  `;
      this.setUpButtons_();

      // Event listener to check if custom color selected
      // to automatically select override button
      document.getElementById('threadColor').addEventListener("change", () => {
        document.getElementById('threadOverride').checked = true;
      });
    }

    setUpButtons_() {
        const saveButton = this.querySelector('#saveThreadOptions');
        saveButton.addEventListener('click', () => {
            const threadOptions = { override: false };
            let input = this.querySelector('input[name="dowhat"]:checked');
            if (input.value === 'threadOverridesFilters') {
                threadOptions.color =
                    this.querySelector('input[name="drawcolor"]').value;
                threadOptions.alpha =
                    this.querySelector('input[name="fillalpha"]').value;
                threadOptions.override = true;
            }

            this.dispatchEvent(new CustomEvent('saveThreadOptionsEvent', {
                detail: threadOptions
            }));
        });
    }
};

window.customElements.define('thread-ui', ThreadUI);

function createThreadChip(thread) {
    const chip = document.createElement('div');
    chip.className = "mdc-chip";
    chip.setAttribute("role", "row");
    chip.style.margin = '5px';
    chip.style.borderRadius = '0px';
    if (thread.enabled_) {
      chip.style.border = thread.drawColor_ ?
        `2px solid ${thread.drawColor_}` : `1px solid black`;
      if (thread.fillAlpha_ > 0) {
        var alpha = Math.min(.6, parseFloat(thread.fillAlpha_) / 100);
        var rgba = thread.drawColor_ + DrawCall.alphaFloatToHex(alpha);
        chip.style.backgroundColor = rgba;
        chip.style.fontWeight = 'bold';
      }
      else {
        chip.style.backgroundColor = `white`;
      }
    }
    else {
      chip.style.border = `1px dashed grey`;
      chip.style.backgroundColor = `white`;
    }

    chip.innerHTML =
      `
    <input checked type="checkbox"/>
    <span role="gridcell">
      <span class="mdc-chip__text" id="thread-name"></span>
    </span>
    <span role="gridcell">
        <i class="material-icons-outlined" style="margin-left: 10px"
          onclick="showEditThreadPopup(this)">edit</i></span>
    </span>
    `;

    chip.querySelector('#thread-name').innerHTML = ("~ "  + thread.threadName_);

    const check = chip.querySelector('input');
    check.addEventListener('change', () => {
      Thread.getThread(thread.threadName_).toggleEnableThread();
      Player.instance.refresh();
    });

    return chip;
}

function showEditThreadPopup(item) {
    var chip = item.closest(".mdc-chip");
    // Slice thread name from HTML so that we exclude the "~ ".
    const threadName = chip.querySelector('#thread-name').innerHTML.slice(2);
    const thread = Thread.getThread(threadName);

    const threadUi = document.createElement('thread-ui');
    threadUi.addEventListener('saveThreadOptionsEvent', (event) => {
      if (event.detail.color && event.detail.alpha) {
        thread.drawColor_ = event.detail.color;
        thread.fillAlpha_ = event.detail.alpha;
        const newThreadChip = createThreadChip(thread);
        const threadFilters = document.querySelector('#threads');
        threadFilters.appendChild(newThreadChip);
        chip.replaceWith(newThreadChip);
      }
      thread.overrideFilters_ = event.detail.override;
      hideModal();
      Player.instance.refresh();
    });
    threadUi.style.position = 'absolute';
    threadUi.style.top = (chip.offsetTop + chip.offsetHeight) + 'px';
    threadUi.style.left = (chip.offsetLeft + 20) + 'px';
    threadUi.style.zIndex = maxZIndex;

    showModal(threadUi);

    var actionform = threadUi.querySelector('#actionform');
    actionform.dowhat.value = thread.overrideFilters_ ?
        'threadOverridesFilters' : 'drawWithFilterOptions';
    actionform.drawcolor.value = thread.drawColor_;
    actionform.fillalpha.value = thread.fillAlpha_;
  }