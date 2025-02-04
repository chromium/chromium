import { lib } from "./index.mjs";

export default {
	"WordSegmenter.segment": {
		func: (model, text) => {
			let provider = lib.DataProvider.compiled();

			var segmenter;
			switch (model) {
				case "Auto":
					segmenter = lib.WordSegmenter.createAuto(provider);
					break;
				case "LSTM":
					segmenter = lib.WordSegmenter.createLstm(provider);
					break;
				case "Dictionary":
					segmenter = lib.WordSegmenter.createDictionary(provider);
			}
			
			let last = 0;
			const iter = segmenter.segment(text);

			const segments = [];
			
			while (true) {
				const next = iter.next();

				if (next === -1) {
					segments.push(text.slice(last));
					break;
				}

				segments.push(text.slice(last, next));
				last = next;
			}
			
			return segments.join(" . ");
		},
		funcName: "WordSegmenter.segment",
		parameters: [
			{
				name: "Model Type (Auto, LSTM, or Dictionary)",
				type: "string",
				defaultValue: "Auto"
			},
			{
				name: "Text",
				type: "string",
				defaultValue: "โดยที่การยอมรับนับถือเกียรติศักดิ์ประจำตัว และสิทธิเท่าเทียมกันและโอนมิได้ของบรรดา สมาชิก ทั้ง หลายแห่งครอบครัว มนุษย์เป็นหลักมูลเหตุแห่งอิสรภาพ ความยุติธรรม และสันติภาพในโลก"
			}
		]
	}
};
