import fs from 'node:fs/promises';
import { PresentationFile, FileBlob } from '@oai/artifact-tool';
const root='/Users/swayamsingal/Desktop/Programming/kyoto-common-contact';
const p=await PresentationFile.importPptx(await FileBlob.load(root+'/output/Common_Broad_Phase_Kyoto_Native_v3.pptx'));
await fs.mkdir(root+'/.presentation-native/final-renders-v3',{recursive:true});
for(let i=0;i<p.slides.items.length;i++){
 const image=await p.export({slide:p.slides.items[i],format:'png',scale:1});
 await fs.writeFile(root+`/.presentation-native/final-renders-v3/slide-${String(i+1).padStart(2,'0')}.png`,new Uint8Array(await image.arrayBuffer()));
}
console.log('Reopened final PPTX; rendered slides:',p.slides.items.length);
