/**
 * Copyright 2021 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
export function log(type: string): (...message: any[]) => void {
    return (...messages: any[]) => {
        const elementId = type + "_log";

        if (!window.document.getElementById(elementId)) {
            window.document.documentElement.innerHTML += `<h3>${type}:</h3><pre id='${elementId}'></pre>`;
            window.document.getElementById(elementId);
        }

        const element = window.document.getElementById(elementId);

        console.log.apply(null, [type].concat(messages));
        element.innerText += messages.join(", ") + "\n";
    }
}
