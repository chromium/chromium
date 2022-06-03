// We reuse the "backend" of the imperative Shadow DOM Distribution API for a new custom element, <my-detail>/<my-summary>.
//TODO(crbug.com/869308):Emulate other <summary><details> features

class MySummaryElement extends HTMLElement {
  constructor() {
    super();
  }
}
customElements.define("my-summary", MySummaryElement);

customElements.define("my-detail", class extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: "open", slotAssignment: "manual" });
  }
  connectedCallback() {
    const target = this;
    if (!target.shadowRoot.querySelector(':scope > slot')) {
      const slot1 = document.createElement("slot");
      const slot2 = document.createElement("slot");
      const child1 = document.createElement("span");
      const child2 = document.createElement("span");
      target.appendChild(child1);
      target.appendChild(child2);
      child1.innerHTML = "&rtrif; ";
      child1.addEventListener('click', (e) => {
        slot2.style.display = "block";
        child2.innerHTML = "&dtrif; ";
        child1.innerText = "";
        slot2.assign(...target.childNodes);
      });
      child2.addEventListener('click', (e) => {
        slot2.style.display = "none";
        child1.innerHTML = "&rtrif; ";
        child2.innerText = "";
        slot1.assign(child1,child2, target.querySelector(':scope > my-summary'));
      });
      const shadowRoot = target.shadowRoot;
      shadowRoot.appendChild(slot1);
      shadowRoot.appendChild(slot2);
      slot1.style.display = "block";
      const observer = new MutationObserver(function(mutations) {
        //Get the first <my-summary> element from <my-detail>'s direct children
        const my_summary = target.querySelector(':scope > my-summary');
        if (my_summary) {
          slot1.assign(child1,child2,my_summary);
        } else {
          slot1.assign(child1,child2);
        }
      });
    observer.observe(this, {childList: true});
    }
  }
});
