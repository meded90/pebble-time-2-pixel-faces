const fs=require('fs'),path=require('path');
const dir=__dirname;
const data=f=>'data:image/png;base64,'+fs.readFileSync(path.resolve(dir,f)).toString('base64');
const assets={original:data('../../resources/images/background.png'),digits:[1,0,0,9].map(n=>data('../../resources/images/digits/digit-'+n+'.png'))};
const html=`<!doctype html><html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Starry Digits · живое небо</title><style>
*{box-sizing:border-box}body{margin:0;background:#10151c;color:#e9edf2;font:16px system-ui}main{max-width:1080px;margin:auto;padding:36px 24px}h1{font-size:32px;margin:8px 0}p{color:#aab7c7;line-height:1.55}.eyebrow{color:#81d9e9;font-size:12px;letter-spacing:2px}.panels{display:flex;gap:32px;flex-wrap:wrap;align-items:start;margin:30px 0}figure{margin:0}figcaption{margin-top:12px;color:#9badbf;font-size:14px}canvas{image-rendering:pixelated;background:black;display:block;max-width:100%;height:auto;border-radius:3px}#animated{width:400px}#original{width:200px}.screen{padding:16px;background:#06080b;border:1px solid #303b49;border-radius:22px;max-width:100%}.controls{display:flex;align-items:center;gap:18px;flex-wrap:wrap;background:#1c2530;padding:18px;border-radius:14px}button,select{font:inherit;color:#f0f4fa;background:#304253;border:1px solid #536779;padding:8px 12px;border-radius:7px;cursor:pointer}label{display:flex;gap:8px;align-items:center}input[type=range]{width:180px;accent-color:#65d5e7}.note{max-width:760px;font-size:14px}.strip{display:flex;overflow:auto;gap:8px;padding:16px 0}.strip button{padding:4px;background:#080d13}.strip canvas{width:80px}.strip button.active{outline:2px solid #65d5e7}#status{font-variant-numeric:tabular-nums;color:#81d9e9}a{color:#81d9e9}</style>
<main><div class="eyebrow">STARRY DIGITS / ANIMATION STUDY 03</div><h1>Живое ночное небо</h1><p>Воздушные потоки Ван Гога · 200 × 228 пикселей · палитра Pebble, до 64 цветов</p>
<div class="panels"><figure><div class="screen"><canvas id="animated" width="200" height="228"></canvas></div><figcaption>Анимация · увеличение <span id="zoomText">2×</span></figcaption></figure><figure><div class="screen"><canvas id="original" width="200" height="228"></canvas></div><figcaption>Исходный фон · 1×</figcaption><p class="note">96 кадров движения исходных полос.<br>Время — оригинальные bitmap-цифры.<br>Луна и город закреплены пиксель в пиксель.</p><p id="status">Загрузка…</p></figure></div>
<div class="controls"><button id="play">Пауза</button><label>Кадр <input id="frame" type="range" min="0" max="95" value="0"></label><label>Скорость <select id="fps"><option value="2">2 кадр/с</option><option value="4">4 кадр/с</option><option value="6">6 кадр/с</option><option value="8">8 кадр/с</option><option value="12" selected>12 кадр/с</option></select></label><label><input id="time" type="checkbox" checked>Время</label><label>Масштаб <select id="zoom"><option value="1">1×</option><option value="2" selected>2×</option><option value="3">3×</option></select></label><button id="save">Скачать кадр PNG</button></div>
<div class="strip" id="strip"></div><p class="note">Исходные полосы перемещаются вдоль изгибов и меняют форму. У соседних полос разные скорость, фаза, поперечный изгиб и переходы синего в бирюзовый. Луна и город остаются исходными. Это HTML-прототип для оценки движения; версия для часов ещё не менялась.</p></main>
<script>const assets=${JSON.stringify(assets)};
const $=id=>document.getElementById(id);let frames=[],index=0,playing=true,last=0,colors=new Set();const ctx=$('animated').getContext('2d');
const load=src=>new Promise((resolve,reject)=>{let i=new Image;i.onload=()=>resolve(i);i.onerror=reject;i.src=src});
function quantize(c){let x=c.getContext('2d'),d=x.getImageData(0,0,200,228);for(let i=0;i<d.data.length;i+=4){for(let k=0;k<3;k++)d.data[i+k]=Math.round(d.data[i+k]/85)*85;d.data[i+3]=255;colors.add(d.data[i]+','+d.data[i+1]+','+d.data[i+2])}x.putImageData(d,0,0);return c}
function canvas(){let c=document.createElement('canvas');c.width=200;c.height=228;return c}
let digits=[],orig;function overlay(c){if(!$('time').checked)return;digits.forEach((d,i)=>c.drawImage(d,8+(i%2)*48,16+Math.floor(i/2)*58,48,58))}
function draw(){ctx.clearRect(0,0,200,228);ctx.drawImage(frames[index],0,0);overlay(ctx);let o=$('original').getContext('2d');o.clearRect(0,0,200,228);o.drawImage(orig,0,0);overlay(o);$('frame').value=index;$('status').textContent='Кадр '+(index+1)+' / 96 · '+colors.size+' цветов фона · цикл '+(96/Number($('fps').value)).toFixed(1)+' с';document.querySelectorAll('.strip button').forEach((b,i)=>b.classList.toggle('active',i===Math.floor(index/8)))}
function pause(){playing=false;$('play').textContent='Воспроизвести'}
// Inverse advection of the original raster in curvilinear coordinates.
// Two staggered flow phases hide resets; transverse waves deform each ribbon.
const N=96,W=200,H=228,TAU=2*Math.PI;
const controls=[[-35,142],[0,160],[35,177],[74,184],[111,179],[142,165],[157,146],[158,128],[169,106],[181,86],[180,73],[168,63],[149,57]];
function curve(points){let out=[];for(let i=0;i<points.length-1;i++){let a=points[Math.max(0,i-1)],b=points[i],c=points[i+1],d=points[Math.min(points.length-1,i+2)];for(let j=0;j<48;j++){let t=j/48,t2=t*t,t3=t2*t;out.push([0,1].map(k=>.5*(2*b[k]+(-a[k]+c[k])*t+(2*a[k]-5*b[k]+4*c[k]-d[k])*t2+(-a[k]+3*b[k]-3*c[k]+d[k])*t3)))}}return out}
const clamp=(x,a,b)=>Math.max(a,Math.min(b,x)),fract=x=>x-Math.floor(x),smooth=x=>{x=clamp(x,0,1);return x*x*(3-2*x)};
function makeFrames(original){let base=canvas(),bc=base.getContext('2d');bc.drawImage(original,0,0);let src=bc.getImageData(0,0,W,H).data,path=curve(controls),cum=[0];for(let i=1;i<path.length;i++)cum[i]=cum[i-1]+Math.hypot(path[i][0]-path[i-1][0],path[i][1]-path[i-1][1]);
let length=cum[cum.length-1],arc=[];for(let q=0;q<=Math.ceil(length);q++){let k=0;while(k<cum.length-2&&cum[k+1]<q)k++;let a=path[k],b=path[k+1],v=clamp((q-cum[k])/(cum[k+1]-cum[k]),0,1),dx=b[0]-a[0],dy=b[1]-a[1],l=Math.hypot(dx,dy)||1;arc.push({x:a[0]+dx*v,y:a[1]+dy*v,nx:-dy/l,ny:dx/l})}
function at(s,d){let q=arc[clamp(Math.round(s),0,arc.length-1)];return [q.x+q.nx*d,q.y+q.ny*d]}
let map=[];for(let y=0;y<H;y++)for(let x=0;x<W;x++){let best=1e9,k=0;for(let j=0;j<arc.length;j++){let q=arc[j],dd=(x-q.x)**2+(y-q.y)**2;if(dd<best){best=dd;k=j}}let q=arc[k],d=(x-q.x)*q.nx+(y-q.y)*q.ny,r=Math.hypot(x-166,y-33),a=Math.atan2(y-33,x-166);let fixed=r<=20||y>=194;let halo=r>20&&r<44&&y<68;let influence=smooth((43-Math.sqrt(best))/12)*smooth((194-y)/9)*smooth((y-55)/14);if(halo)influence=smooth((r-20)/4)*smooth((44-r)/7);map.push({x,y,s:k,d,r,a,fixed,halo,influence})}
function sample(x,y){x=clamp(x,0,W-1);y=clamp(y,0,H-1);let x0=Math.floor(x),y0=Math.floor(y),x1=Math.min(x0+1,W-1),y1=Math.min(y0+1,H-1),fx=x-x0,fy=y-y0;let out=[];for(let k=0;k<3;k++)out[k]=(src[(y0*W+x0)*4+k]*(1-fx)+src[(y0*W+x1)*4+k]*fx)*(1-fy)+(src[(y1*W+x0)*4+k]*(1-fx)+src[(y1*W+x1)*4+k]*fx)*fy;return out}
function render(f){let c=canvas(),cx=c.getContext('2d'),im=cx.createImageData(W,H),t=(f%N)/N;for(let p=0;p<map.length;p++){let m=map[p],i=p*4;im.data[i+3]=255;let rgb=[src[i],src[i+1],src[i+2]];if(!m.fixed&&m.influence>0){
// Different ribbons have different travel distance and phase, continuously blended across lanes.
let lane=m.halo?(m.r-20)/4:m.d/5;
let offset=.09*Math.sin(lane*.91)+lane*.035;
let travel=20+6*Math.sin(lane*.87+.6);
let u=fract(t+offset),v=fract(t+offset+.5),weight=Math.sin(Math.PI*u)**2;
let deform=1.7*Math.sin(m.s*.072-TAU*t*2+lane*.52)+.7*Math.sin(m.s*.13-TAU*t*3-lane*.77);
function adv(age){if(m.halo){let radius=m.r+1.3*Math.sin(m.a*3-TAU*t*2+lane);let angle=m.a-(age-.5)*(.7+.2*Math.sin(lane));return sample(166+radius*Math.cos(angle),33+radius*Math.sin(angle))}let pos=at(m.s-(age-.5)*travel,m.d-deform);return sample(pos[0],pos[1])}
let a=adv(u),b=adv(v);let moved=a.map((n,k)=>n*weight+b[k]*(1-weight));
// Color travels with each stripe, without adding a separate highlight layer.
let tint=.14*Math.sin(m.s*.039-TAU*t+lane*1.63);if(moved[2]>moved[0]+20){moved[1]=clamp(moved[1]+tint*(moved[2]-moved[0]),0,255)}
rgb=rgb.map((n,k)=>n*(1-m.influence)+moved[k]*m.influence);
}for(let k=0;k<3;k++)im.data[i+k]=Math.round(rgb[k]/85)*85}cx.putImageData(im,0,0);return c}
for(let f=0;f<N;f++)frames.push(quantize(render(f)));
let zero=frames[0].getContext('2d').getImageData(0,0,W,H).data,close=render(N).getContext('2d').getImageData(0,0,W,H).data,seam=0,staticChanged=0,changes=[];for(let i=0;i<zero.length;i++)if(zero[i]!==close[i])seam++;
for(let f=0;f<N;f++){let pix=frames[f].getContext('2d').getImageData(0,0,W,H).data,next=frames[(f+1)%N].getContext('2d').getImageData(0,0,W,H).data,delta=0;for(let p=0;p<map.length;p++){for(let k=0;k<3;k++){if(map[p].fixed&&pix[p*4+k]!==src[p*4+k])staticChanged++;delta+=Math.abs(pix[p*4+k]-next[p*4+k])}}changes.push(delta)}
window.flowChecks={seamByteDifferences:seam,staticPixelChannelDifferences:staticChanged,frames:N,wrapDelta:changes[N-1],minStep:Math.min(...changes),maxStep:Math.max(...changes)};
}

Promise.all([load(assets.original),...assets.digits.map(load)]).then(([original,...ds])=>{orig=original;digits=ds;makeFrames(original);frames.forEach((f,i)=>{if(i%8!==0)return;let b=document.createElement('button'),v=canvas();v.getContext('2d').drawImage(f,0,0);b.append(v);b.title='Кадр '+(i+1);b.setAttribute('aria-label',b.title);b.onclick=()=>{pause();index=i;draw()};$('strip').append(b)});window.previewQA={frameCount:frames.length,paletteSize:colors.size,width:200,height:228};document.querySelector('.note').insertAdjacentHTML('beforeend','<br>Проверка: неподвижные слои — '+window.flowChecks.staticPixelChannelDifferences+' изменений; стык цикла — '+window.flowChecks.seamByteDifferences+' отличий.');draw();requestAnimationFrame(tick)}).catch(e=>{$('status').textContent='Ошибка загрузки: '+e.message});
function tick(t){if(playing&&t-last>=1000/Number($('fps').value)){index=(index+1)%96;last=t;draw()}requestAnimationFrame(tick)}
$('play').onclick=()=>{playing=!playing;$('play').textContent=playing?'Пауза':'Воспроизвести';last=performance.now()};$('frame').oninput=e=>{pause();index=Number(e.target.value);draw()};$('time').onchange=draw;$('fps').onchange=draw;$('zoom').onchange=e=>{$('animated').style.width=200*Number(e.target.value)+'px';$('zoomText').textContent=e.target.value+'×'};$('save').onclick=()=>{let a=document.createElement('a');a.download='starry-sky-frame-'+String(index+1).padStart(2,'0')+'.png';a.href=frames[index].toDataURL();a.click()};
</script></html>`;
fs.writeFileSync(path.join(dir,'index.html'),html);
console.log('Created self-contained HTML: '+path.join(dir,'index.html'));
