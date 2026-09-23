import createCP80Module from './cp80-wasm.js';

const LINE_OUTPUT_GAIN = 88.0;
const MAX_BLOCK = 2048;

class CP80Processor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.module = null;
    this.outputPtr = 0;
    this.ready = false;
    this.pending = [];
    this.port.onmessage = ({ data }) => {
      if (data.type === 'event') {
        if (this.ready) this.apply(data);
        else this.pending.push(data);
      }
    };

    createCP80Module().then((module) => {
      this.module = module;
      module._cp80_web_prepare(sampleRate, MAX_BLOCK);
      this.outputPtr = module._cp80_web_output_ptr();
      this.ready = true;
      for (const event of this.pending) this.apply(event);
      this.pending = [];
      this.port.postMessage({ type: 'ready' });
    }).catch((error) => {
      this.port.postMessage({ type: 'error', message: String(error) });
    });
  }

  apply(event) {
    switch (event.kind) {
      case 'noteOn': this.module._cp80_web_note_on(event.note, event.velocity); break;
      case 'noteOff': this.module._cp80_web_note_off(event.note); break;
      case 'sustain': this.module._cp80_web_sustain(event.on ? 1 : 0); break;
    }
  }

  process(_inputs, outputs) {
    const channels = outputs[0];
    if (!channels || !channels[0]) return true;
    const left = channels[0];
    const right = channels[1] || channels[0];
    left.fill(0);
    if (channels[1]) right.fill(0);
    if (!this.ready || left.length > MAX_BLOCK) return true;

    const n = this.module._cp80_web_process(left.length);
    const source = this.module.HEAPF32.subarray(this.outputPtr >> 2,
                                                 (this.outputPtr >> 2) + n);
    for (let i = 0; i < n; ++i) {
      const value = source[i] * LINE_OUTPUT_GAIN;
      left[i] = value;
      if (channels[1]) right[i] = value;
    }
    return true;
  }
}

registerProcessor('cp80-model', CP80Processor);
