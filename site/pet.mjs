// Original Dash-Chan art; no imported sprites. Buddy shapes are generated from include/avatars.h.
import {buddyBody,buddyDetails,buddyTones,makePen} from './buddy_art.mjs';
const rgb=c=>`rgb(${Math.round((c>>11)*255/31)},${Math.round(((c>>5)&63)*255/63)},${Math.round((c&31)*255/31)})`;
export function quantize(hex){const c=parseInt(hex.replace('#',''),16);return rgb(((c>>19)<<11)|(((c>>10)&63)<<5)|((c>>3)&31));}
export function drawPet(canvas,options={},time=0){
  const {base=0,body='#fff3d2',accent='#ff88b6',eyes=0,eye='#101029',headwear=0,eyewear=0,neckwear=0,mood=0,custom=false,field=0,world=null}=options;
  const g=canvas.getContext('2d');
  g.clearRect(0,0,canvas.width,canvas.height);if(world!==null)drawWorld(g,canvas.width,canvas.height,world,time,field===2);g.save();
  const scale=Math.min(canvas.width/220,canvas.height/165);
  g.translate(canvas.width/2,canvas.height/2+2+Math.sin(time/650)*2*scale);g.scale(scale,scale);
  const ink=rgb(0x1085),paper=rgb(0xff7a),mint=rgb(0x87b6),peach=rgb(0xfdaf),lilac=rgb(0xc5ff),blush=custom?quantize(accent):rgb(base===3?0xc5ff:base===4?0xff28:0xfbb2);
  const skin=custom?quantize(body):[paper,mint,lilac,'#000000',rgb(0xfa64)][base];
  const phase=Math.floor(time/150);
  const ellipse=(x,y,rx,ry,c)=>{g.fillStyle=c;g.beginPath();g.ellipse(x,y,rx,ry,0,0,Math.PI*2);g.fill();};
  const rect=(x,y,w,h,r,c)=>{g.fillStyle=c;g.beginPath();g.roundRect(x,y,w,h,r);g.fill();};
  const tri=(x,y,x2,y2,x3,y3,c)=>{g.fillStyle=c;g.beginPath();g.moveTo(x,y);g.lineTo(x2,y2);g.lineTo(x3,y3);g.closePath();g.fill();};
  const line=(x,y,x2,y2,c)=>{g.strokeStyle=c;g.lineWidth=1.4;g.beginPath();g.moveTo(x,y);g.lineTo(x2,y2);g.stroke();};
  if(mood===4)g.translate(Math.sin(time/65)*4,Math.cos(time/90)*4);
  if(field===2){for(let i=0;i<4;i++){const x=-82+(Math.floor(time/25)+i*47)%164;line(x,45,Math.min(87,x+9),45,'#344557');}}
  // Body, markings and face come from the firmware's own art (site/buddy_art.mjs).
  const to565=hex=>{const c=parseInt(hex.replace('#',''),16);return ((c>>19)<<11)|(((c>>10)&63)<<5)|((c>>3)&31);};
  const tones=buddyTones(base,custom,custom?to565(body):0,custom?to565(accent):0);
  const gaze=mood===4?Math.sin(time/100)*5:Math.sin(time/2100)*3;
  const blink=time%4100<130&&mood!==4;
  ellipse(0,47,50,6,rgb(0x18e7));
  for(const grow of [2,0])buddyBody(makePen(g,grow,tones.line),base,tones.body,tones.trim,phase);
  buddyDetails(makePen(g,0,tones.line),base,mood,gaze,blink,custom,custom?eyes:0,custom?to565(eye):0x1085,tones.body,tones.blush,tones.line,tones.shade,tones.light,tones.trim,phase);
  const gy=mood===3||mood===4||mood===5?-25:-4;
  if(eyewear===1){
    for(const side of [-1,1])for(let a=0;a<48;a++){const angle=a*2*Math.PI/48;ellipse(side*21+Math.round(Math.cos(angle)*13),gy+Math.round(Math.sin(angle)*14),1,1,blush);}
    line(-8,gy,8,gy,blush);
  }else if(eyewear===2||eyewear===3){
    rect(-39,gy-12,78,24,6,blush);
    if(eyewear===2){rect(-35,gy-8,31,17,4,ink);rect(4,gy-8,31,17,4,ink);line(-28,gy-5,-20,gy+3,paper);line(12,gy-5,20,gy+3,paper);}
    else{rect(-35,gy-8,70,16,4,ink);for(let x=-27;x<=27;x+=9)rect(x,gy-3,4,6,0,mint);}
  }
  if(headwear===1){tri(-6,-34,-23,-44,-23,-24,blush);tri(6,-34,23,-44,23,-24,blush);ellipse(0,-34,6,6,blush);}
  else if(headwear===2){rect(-22,-42,44,10,0,peach);tri(-22,-42,-22,-59,-8,-42,peach);tri(-10,-42,0,-61,10,-42,peach);tri(8,-42,22,-59,22,-42,peach);}
  else if(headwear===3||headwear===5){ellipse(0,-38,35,17,blush);rect(-38,-38,headwear===5?83:76,10,4,blush);if(headwear===3)ellipse(0,-57,7,7,peach);else ellipse(8,-44,5,5,paper);}
  else if(headwear===4){for(let i=0;i<5;i++){const a=i*2*Math.PI/5;ellipse(29+Math.trunc(Math.cos(a)*9),-33+Math.trunc(Math.sin(a)*9),6,6,blush);}ellipse(29,-33,5,5,peach);}
  if(neckwear===1){rect(-37,31,74,8,0,blush);rect(22,31,12,22,4,blush);}
  else if(neckwear===2){tri(-30,30,30,30,0,48,blush);ellipse(0,36,3,3,paper);}
  else if(neckwear===3){tri(-4,33,-21,24,-21,42,blush);tri(4,33,21,24,21,42,blush);ellipse(0,33,5,5,peach);}
  if(field){
    const phase=Math.floor(time/150),signal=mood===4?rgb(0xf9c7):mint;
    const lift=field===2?(phase%6<3?-2:1):0,ax=-61,ay=6+lift;
    line(ax+11,ay+21,ax-5,ay-14,paper);line(ax-16,ay-10,ax+12,ay-20,paper);
    line(ax-12,ay-2,ax+15,ay-12,paper);line(ax-7,ay+6,ax+17,ay-4,paper);
    rect(ax+6,ay+15,8,18,3,blush);ellipse(-45,29+lift,10,8,skin);
    line(52,-3,58,13,paper);rect(39,10,32,37,5,blush);rect(43,14,24,23,4,ink);
    line(55,32,46+(phase%8)*2,17,signal);ellipse(59,23,2,2,signal);
    ellipse(49,41,2,2,paper);ellipse(59,41,2,2,signal);line(66,11,59,-9,blush);
    rect(49,-17,10,19,4,ink);rect(52,-13,4,11,0,blush);ellipse(40,31,9,8,skin);
  }
  if(mood===5){
    const colors=['#ffdb68','#72ffdb','#ff7ebd','#bca0ff'];
    for(let i=0;i<10;i++){const x=-87+(i*37)%174,y=-65+(i*23+Math.floor(time/35))%107;rect(x,y,3,4,0,colors[i%4]);}
  }else if(mood===3){
    for(const side of [-1,1]){const x=side*70,y=-28-(Math.floor(time/140)+side*5)%17;ellipse(x-3,y-2,4,4,blush);ellipse(x+3,y-2,4,4,blush);tri(x-7,y,x+7,y,x,y+8,blush);}
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
