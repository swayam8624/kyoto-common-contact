import fs from 'node:fs/promises';
import {PresentationFile,FileBlob} from '@oai/artifact-tool';
const root='/Users/swayamsingal/Desktop/Programming/kyoto-common-contact';
const p=await PresentationFile.importPptx(await FileBlob.load(root+'/output/Common_Broad_Phase_Kyoto_3D.pptx'));
const image=await p.export({slide:p.slides.items[6],format:'png',scale:1});
await fs.writeFile(root+'/.presentation-3d/reopened-slide-7.png',new Uint8Array(await image.arrayBuffer()));console.log('Reopened exact final PPTX and rendered slide 7 with embedded PNGs; slides:',p.slides.items.length);
