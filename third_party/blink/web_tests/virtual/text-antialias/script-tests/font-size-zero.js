var s = document.createElement('span');
s.style.fontSize = 0;
s.innerHTML = 'Text';
document.body.appendChild(s);

shouldBe("s.getBoundingClientRect().height", "0");
shouldBe("s.getBoundingClientRect().width", "0");

document.body.removeChild(s);
