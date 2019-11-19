# Lottie Web Worker

#### Using the lottie player on a worker thread and an offscreen canvas.

## Sample usage
### 1. Setting up a new animation
#### HTML:
```html
<canvas id="a"></canvas>
```

#### Javascript:
```js
let offscreenCanvas = document.getElementById('a').transferControlToOffscreen();
let animationData = JSON_LOTTIE_ANIMATION_DATA;

let worker = new Worker('lottie_worker.min.js');
worker.postMessage({
  canvas: offscreenCanvas,
  animationData: animationData,
  drawSize: {
    width: 200,
    height: 100
  }
  params: {
    loop: true,
    autoplay: true
  }
})
```

### 2. Updating the size of the canvas
```js
worker.postMessage({
  drawSize: {
    width: 250,
    height: 150
  }
})
```

### 3. Pausing the animation
```js
worker.postMessage({
  control: {
    play: false
  }
})
```

## Message field description
```python
data: {
  canvas: 'The offscreen canvas that will display the animation.',
  animationData: 'The json lottie animation data.',
  drawSize: {
    width: 'The width of the rendered frame in pixels',
    height: 'The height of the rendered frame in pixels',
  },
  params: {
    loop: 'Set "true" for a looping animation',
    autoplay: 'Set "true" for the animation to autoplay on load',
  },
  control: {
    play: 'Set "true" to play a paused animation or "false" to pause a playing animation',
  }
},
```

## Events fired back to the parent thread
The web worker running the lottie player will send the following events back to
its parent thread:
1. **'initialized'**
```javascript
{
    name: 'initialized',
    success: true/false // True if the animation was successfully initialized.
}
```
2. **'playing'**
```javascript
{
    name: 'playing'
}
```
3. **'paused'**
```javascript
{
    name: 'paused'
}
```
4. **'resized'**
```javascript
{
    name: 'resized',
    size: {
        height: HEIGHT, // Current height of canvas in pixels.
        width: WIDTH    // Current width of canvas in pixels.
    }
}
```
