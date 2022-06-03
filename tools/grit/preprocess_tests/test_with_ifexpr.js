Polymer({
  is: 'cr-foo',

  _template: html`
    <if expr="bar">
      <button on-click="onClick_">I should be included in HTML</button>
    </if>
    <if expr="apple">
      <div>I should be excluded from HTML</div>
    </if>
  `,

  onClick_() {
    // <if expr="orange">
    console.log('I should be excluded from JS');
    // </if>
    // <if expr="foo">
    console.log('I should be included in JS');
    // </if>
  }
});
