// Reproduce the approved HTML flow renderer without a browser or npm packages.
const fs=require('fs'),path=require('path'),vm=require('vm'),cp=require('child_process');
const root=path.resolve(__dirname,'..'),repo=path.resolve(root,'..');
const python=process.env.PYTHON||path.join(repo,'.venv/bin/python');
const raw=cp.execFileSync(python,['-c','from PIL import Image; import sys; sys.stdout.buffer.write(Image.open(sys.argv[1]).convert("RGBA").tobytes())',path.join(root,'resources/images/background.png')]);
function canvas(){let data=new Uint8ClampedArray(200*228*4);const ctx={drawImage(img){data.set(img)},getImageData(){return {data:new Uint8ClampedArray(data)}},createImageData(){return {data:new Uint8ClampedArray(data.length)}},putImageData(img){data.set(img.data)}};return {getContext(){return ctx}}}
const source=fs.readFileSync(path.join(root,'previews/animated-sky-v3/build-preview.cjs'),'utf8');
let code=source.slice(source.indexOf('// Inverse advection'),source.indexOf('Promise.all([load(assets.original)'));
// Reverse the phase and smoothly remove the geometric displacement at the end.
// The final sample coordinates are the original pixel coordinates: no crossfade.
code=code.replace('t=(f%N)/N','t=(N-1-f)/N,settle=1-smooth((f-(N-17))/16)')
 .replace('!m.fixed&&m.influence>0','!m.fixed&&m.influence>0&&settle>0')
 .replace('m.r+1.3*Math.sin','m.r+settle*1.3*Math.sin')
 .replace('m.a-(age-.5)*(.7+.2*Math.sin(lane))','m.a-settle*(age-.5)*(.7+.2*Math.sin(lane))')
 .replace('let pos=at(m.s-(age-.5)*travel,m.d-deform);return sample(pos[0],pos[1])','let center=at(m.s,m.d),pos=at(m.s-settle*(age-.5)*travel,m.d-settle*deform);return sample(m.x+pos[0]-center[0],m.y+pos[1]-center[1])')
 .replace('let tint=.14*Math.sin','let tint=settle*.14*Math.sin');
const frames=[],context={canvas,frames,quantize:x=>x,window:{},original:raw};vm.createContext(context);vm.runInContext(code+'\nmakeFrames(original);',context);
const packed=frames.map(f=>{let p=f.getContext().getImageData().data;return Buffer.from(Array.from({length:45600},(_,i)=>0xc0|((p[i*4]/85)<<4)|((p[i*4+1]/85)<<2)|(p[i*4+2]/85)))});
const count=Number(process.env.INTRO_FRAME_COUNT||46);
const selected=Array.from({length:count},(_,i)=>packed[Math.round(i*(packed.length-1)/(count-1))]);
const still=Buffer.from(Array.from({length:45600},(_,i)=>0xc0|((raw[i*4]/85)<<4)|((raw[i*4+1]/85)<<2)|(raw[i*4+2]/85)));
if(!selected[selected.length-1].equals(still))throw Error('Last frame must equal the original background');
fs.mkdirSync(path.join(root,'build'),{recursive:true});
fs.writeFileSync(path.join(root,'build/intro-reference.bin'),Buffer.concat(selected));
fs.mkdirSync(path.join(root,'resources/data'),{recursive:true});
const output=path.join(root,'resources/data/intro.bin');
require('./encode-intro.cjs')(selected, output, path.join(root,'src/c/intro_config.h'));
console.log(JSON.stringify({frames:selected.length,nominalFrameHoldMs:Math.round(4000/count)*count,resourceBytes:fs.statSync(output).size,fixedRegionChanges:context.window.flowChecks.staticPixelChannelDifferences,finalMatchesOriginal:true},null,2));
