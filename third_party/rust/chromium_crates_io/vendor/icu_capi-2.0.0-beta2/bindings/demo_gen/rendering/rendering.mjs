function generateTemplate(className, variable, selector) {
    if (className[variable] === undefined) {
        className[variable] = document.querySelector(selector).content;
    }
}

class ParameterTemplate extends HTMLElement {
    default = null;

    inputElement = null;

    static baseTemplate;
    constructor(options = {}, className, selector, defaultValue=null, ...args) {
        super();
        generateTemplate(ParameterTemplate, "baseTemplate", "#parameter");
        generateTemplate(className, "template", selector);
        let baseClone = ParameterTemplate.baseTemplate.cloneNode(true);

        let clone = className["template"].cloneNode(true);

        this.initialize(clone, ...args);

        this.inputElement = clone.querySelector("*[data-oninput]");
        if (this.inputElement !== null) {
            this.inputElement.addEventListener("input", this.input.bind(this));
        }
        
        clone.slot = "parameter";
        baseClone.appendChild(clone);

        const shadowRoot = this.attachShadow({ mode: "open" });
        shadowRoot.appendChild(baseClone);

        if ("defaultValue" in options) {
            this.default = options.defaultValue;
            this.setValue(options.defaultValue);
        } else if (this.default === null) {
            this.default = defaultValue;
        }
    }

    setValue(v) {
        if (this.inputElement !== null) {
            this.inputElement.value = v;
        }
    }

    getEventValue(event) {
        return event.target.value;
    }

    input(event) {
        this.dispatchEvent(new CustomEvent("parameter-input", {
            detail: this.getEventValue(event)
        }));
    }

    initialize(clone) {

    }
}

customElements.define("terminus-param", ParameterTemplate);

class BooleanTemplate extends ParameterTemplate {
    static template;
    constructor(options) {
        super(options, BooleanTemplate, "template#boolean", false);
    }

    getEventValue(event) {
        return event.target.checked;
    }

    setValue(v) {
        this.inputElement.checked = v;
    }
}

customElements.define("terminus-param-boolean", BooleanTemplate);

class NumberTemplate extends ParameterTemplate {
    static template;
    constructor(options) {
        super(options, NumberTemplate, "template#number", 0);
    }
    
    getEventValue(event) {
        return parseFloat(event.target.value);
    }
}

customElements.define("terminus-param-number", NumberTemplate);

class StringTemplate extends ParameterTemplate {
    static template;
    constructor(options) {
        super(options, StringTemplate, "template#string", "");
    }
}

customElements.define("terminus-param-string", StringTemplate);

class StringArrayTemplate extends ParameterTemplate {
    static template;
    constructor(options) {
        super(options, StringArrayTemplate, "template#string-array", []);
    }

    getEventValue(event) {
        return event.target.value.split(",");
    }
}

customElements.define("terminus-param-string-array", StringArrayTemplate);

class EnumOption extends HTMLElement {
    static template;
    constructor(optionText) {
        super();
        generateTemplate(EnumOption, "template", "template#enum-option");
        let clone = EnumOption.template.cloneNode(true);

        clone.querySelector("slot[name='option-text']").parentElement.innerText = optionText;
        
        this.append(...clone.children);
    }
}

customElements.define("terminus-enum-option", EnumOption);

class EnumTemplate extends ParameterTemplate {
    static template;

    #enumType;
    constructor(options, enumType) {
        super(options, EnumTemplate, "template#enum", null, enumType);
        this.#enumType = enumType;
    }

    initialize(clone, enumType) {
        let options = clone.querySelector("*[data-options]");
        
        for (let entry of enumType.getAllEntries()) {
            
            if (this.default === null) {
                this.default = entry[0];
            }
            options.append(...(new EnumOption(entry[0])).children);
        }
    }

    getEventValue(event) {
        return this.#enumType[event.target.value];
    }
}

customElements.define("terminus-param-enum", EnumTemplate);

class TerminusParams extends HTMLElement {
    #params = [];

    constructor(library, evaluateExternal, params){
        super();

        for (var i = 0; i < params.length; i++) {
            let param = params[i];
            let paramName = document.createElement("span");
            paramName.slot = "param-name";
            paramName.innerText = param.name;

            var newChild;

            switch (param.typeUse) {
                case "string":
                    newChild = new StringTemplate(param);
                    this.#params[i] = "";
                    break;
                case "boolean":
                    newChild = new BooleanTemplate(param);
                    this.#params[i] = false;
                    break;
                case "number":
                    newChild = new NumberTemplate(param);
                    this.#params[i] = 0;
                    break;
                case "Array<string>":
                    newChild = new StringArrayTemplate(param);
                    this.#params[i] = [];
                    break;
                case "enumerator":
                    newChild = new EnumTemplate(param, library[param.type]);
                    this.#params[i] = newChild.default
                    break;
                case "external":
                    let updateParamEvent = (value) => {
                        this.#params[i] = value;
                    };
                    evaluateExternal(param, updateParamEvent);
                    break;
                default:
                    console.error("Unrecognized parameter: ", param);
                    break;
            }

            newChild.addEventListener("parameter-input", this.input.bind(this, i));
            this.#params[i] = newChild.default;

            newChild.appendChild(paramName);
            this.appendChild(newChild);
        }
    }

    input(paramIdx, event) {
        this.#params[paramIdx] = event.detail;
    }

    get paramArray() {
        return this.#params;
    }
}

customElements.define("terminus-params", TerminusParams);

export class TerminusRender extends HTMLElement {
    static template;

    #func = null;
    #parameters;
    #output;
    constructor(library, evaluateExternal, terminus) {
        super();
        generateTemplate(TerminusRender, "template", "template#terminus");
        let clone = TerminusRender.template.cloneNode(true);

        this.id = terminus.funcName;

        this.#func = terminus.func;

        let button = clone.querySelector("*[data-submit]");
        button.addEventListener("click", this.submit.bind(this));

        let funcText = document.createElement("span");
        funcText.slot = "func-name";
        funcText.innerText = terminus.funcName;
        this.appendChild(funcText);

        this.#parameters = new TerminusParams(library, evaluateExternal, terminus.parameters);
        this.#parameters.slot = "parameters";
        this.appendChild(this.#parameters);

        this.#output = document.createElement("span");
        this.#output.slot = "output";

        this.appendChild(this.#output);

        const shadowRoot = this.attachShadow({ mode: "open" });
        shadowRoot.appendChild(clone);
    }

    submit() {
        try {
            this.#output.innerText = this.#func(...this.#parameters.paramArray);
        } catch(e) {
            this.#output.innerText = e;
            throw e;
        }
    }
}

customElements.define("terminus-render", TerminusRender);