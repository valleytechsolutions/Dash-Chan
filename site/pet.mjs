// Original Dash-Chan art; no imported sprites. The buddy itself (body, face, gear, aura and the
// order they are layered in) is generated from include/avatars.h into buddy_art.mjs.
import {buddyArt,buddyTones,makePen} from './buddy_art.mjs';
const rgb=c=>`rgb(${Math.round((c>>11)*255/31)},${Math.round(((c>>5)&63)*255/63)},${Math.round((c&31)*255/31)})`;
export const to565=hex=>{const c=parseInt(hex.replace('#',''),16);return ((c>>19)<<11)|(((c>>10)&63)<<5)|((c>>3)&31);};
export function quantize(hex){return rgb(to565(hex));}
// The firmware's Look for a studio profile (colours become the LCD's RGB565).
export function lookOf({base=0,body='#fff3d2',accent='#ff88b6',eye='#101029',eyes=0,headwear=0,eyewear=0,neckwear=0,mark=0,back=0,held=0,aura=0,custom=false}={}){
  return {base,custom,body:to565(body),accent:to565(accent),eye:to565(eye),eyes,headwear,eyewear,neckwear,mark,back,held,aura};
}
// options: a studio profile plus mood, field (0 none, 1 kit, 2 cruising), world (null = clear),
// still (no idle motion, for thumbnails), and zoom with focusX/focusY (the buddy-space point to
// centre on) for close-ups. zoom 1 shows the whole buddy with room for wings and aura.
export function drawPet(canvas,options={},time=0){
  const {mood=0,field=0,world=null,still=false,zoom=1,focusX=0,focusY=0}=options;
  const g=canvas.getContext('2d');
  g.clearRect(0,0,canvas.width,canvas.height);if(world!==null)drawWorld(g,canvas.width,canvas.height,world,time,field===2);g.save();
  const scale=Math.min(canvas.width/220,canvas.height/165)*zoom;
  g.translate(canvas.width/2,canvas.height/2+2+(still?0:Math.sin(time/650)*2*scale));g.scale(scale,scale);g.translate(-focusX,-focusY);
  const phase=Math.floor(time/150);
  const rect=(x,y,w,h,r,c)=>{g.fillStyle=c;g.beginPath();g.roundRect(x,y,w,h,r);g.fill();};
  const line=(x,y,x2,y2,c)=>{g.strokeStyle=c;g.lineWidth=1.4;g.beginPath();g.moveTo(x,y);g.lineTo(x2,y2);g.stroke();};
  if(mood===4&&!still)g.translate(Math.sin(time/65)*4,Math.cos(time/90)*4);
  if(field===2){for(let i=0;i<4;i++){const x=-82+(Math.floor(time/25)+i*47)%164;line(x,45,Math.min(87,x+9),45,'#344557');}}
  const look=lookOf(options),tones=buddyTones(look.base,look.custom,look.body,look.accent);
  const gaze=still?0:mood===4?Math.sin(time/100)*5:Math.sin(time/2100)*3;
  const blink=!still&&time%4100<130&&mood!==4;
  buddyArt(makePen(g,0,tones.line),makePen(g,2,tones.line),makePen(g,1,tones.line),look,tones,mood,gaze,blink,field,phase);
  // Mood dressing for the preview only; on the robot the home screen draws its own.
  if(mood===5){
    const colors=['#ffdb68','#72ffdb','#ff7ebd','#bca0ff'];
    for(let i=0;i<10;i++){const x=-87+(i*37)%174,y=-65+(i*23+Math.floor(time/35))%107;rect(x,y,3,4,0,colors[i%4]);}
  }else if(mood===3){
    const pen=makePen(g,0,tones.line);
    for(const side of [-1,1]){const x=side*70,y=-28-(Math.floor(time/140)+side*5)%17;pen.oval(x-3,y-2,4,4,tones.blush);pen.oval(x+3,y-2,4,4,tones.blush);pen.tri(x-7,y,x+7,y,x,y+8,tones.blush);}
  }
  g.restore();
}

// Original arcade habitats; scenery is decorative, never geographic or directional.
export function drawWorld(g,width,height,world,time,moving=false){
  g.save();g.scale(width/320,height/240);
  const palettes=[['#25236b','#6553c6','#24dfd0'],['#812e92','#f78b78','#ffd75e'],['#17275d','#4e3fa1','#87d5ff'],['#137e91','#69c5b5','#e0ff91']];
  const [a,b,pop]=palettes[world%4],pink='#ff79c6';
  const sky=g.createLinearGradient(0,0,0,240);sky.addColorStop(0,a);sky.addColorStop(1,b);g.fillStyle=sky;g.fillRect(0,0,320,240);
  const rect=(x,y,w,h,c)=>{g.fillStyle=c;g.fillRect(Math.round(x),Math.round(y),w,h);};
  const circle=(x,y,r,c)=>{g.fillStyle=c;g.beginPath();g.arc(x,y,r,0,Math.PI*2);g.fill();};
  const line=(x,y,x2,y2,c)=>{g.strokeStyle=c;g.lineWidth=1;g.beginPath();g.moveTo(x,y);g.lineTo(x2,y2);g.stroke();};
  const tri=(x,y,x2,y2,x3,y3,c)=>{g.fillStyle=c;g.beginPath();g.moveTo(x,y);g.lineTo(x2,y2);g.lineTo(x3,y3);g.fill();};
  const star=(x,y,c)=>{rect(x-3,y,8,2,c);rect(x,y-3,2,8,c);};
  const cloud=(x,y)=>{rect(x,y,46,9,'#dcefff');rect(x+10,y-8,21,13,'#dcefff');rect(x+29,y-4,10,8,'#dcefff');};
  if(world===0){
    circle(255,45,26,pink);
    for(let i=0;i<14;i++){const x=(i*30+400-(moving?time/100%320:0))%420-50,y=100+i*13%44;rect(x,y,25,100,'#29265e');rect(x,y,25,2,i%2?pop:pink);for(let k=0;k<4;k++)for(let j=0;j<2;j++)rect(x+5+j*11,y+9+k*13,4,5,(i+j+k)%3?'#426ba4':pop);}
    g.strokeStyle='#50d4ce';for(const r of [12,24,36]){g.beginPath();g.arc(45,51,r,0,Math.PI*2);g.stroke();}line(45,51,45,115,pop);circle(45,51,4,pop);star(161,29,pop);
  }else if(world===1){
    circle(250,58,38,pop);for(let y=58;y<96;y+=8)rect(210,y,80,3,b);cloud(24+time/1700%20,38);
    tri(-20,185,67,94,180,185,'#8143a7');tri(130,185,252,108,360,185,'#b65baf');tri(50,112,67,94,91,112,pink);rect(0,177,320,36,'#895f9f');
    for(let i=0;i<12;i++)rect((i*47+time/180)%320,184+i%3*7,20,2,pop);
  }else if(world===2){
    for(let i=0;i<24;i++){const x=(i*47+21)%310+4,y=(i*29+9)%167+10;if(i%4)rect(x,y,2,2,pop);else star(x,y,(i+Math.floor(time/1300))%4?'#7a8ac8':pop);}
    g.strokeStyle=pink;g.beginPath();g.ellipse(255,57,43,13,0,0,Math.PI*2);g.stroke();circle(255,54,26,pink);circle(246,45,9,'#ffb7e2');circle(265,65,7,'#bb62b8');line(213,63,296,50,pop);circle(26,190,36,'#467598');circle(16,177,7,'#365776');
  }else{
    cloud(225-time/1700%20,41);cloud(23,71);circle(43,32,19,'#ffe878');circle(52,218,80,'#2caa89');circle(235,222,92,'#4ec599');
    for(const x of [18,290]){rect(x,155,4,45,'#1e735d');rect(x-9,170,13,5,pop);rect(x-12,144,28,15,pink);rect(x-4,138,12,27,pink);rect(x-2,148,8,8,'#ffeb91');}
  }
  rect(0,208,320,32,'#2a2859');rect(0,208,320,3,'#68f9d0');
  const offset=moving?time/80%32:0;for(let x=-20;x<350;x+=32)line(x-offset,211,x-offset-14,240,'#4c5486');line(0,225,320,225,'#4c5486');
  g.restore();
}
