// Original Dash-Chan art. Geometry mirrors include/avatars.h; no imported sprites.
const rgb=c=>`rgb(${Math.round((c>>11)*255/31)},${Math.round(((c>>5)&63)*255/63)},${Math.round((c&31)*255/31)})`;
export function quantize(hex){const c=parseInt(hex.replace('#',''),16);return rgb(((c>>19)<<11)|(((c>>10)&63)<<5)|((c>>3)&31));}
export function drawPet(canvas,options={},time=0){
  const {base=0,body='#fff3d2',accent='#ff88b6',eyes=0,eye='#101029',headwear=0,eyewear=0,neckwear=0,mood=0,custom=false,field=0}=options;
  const g=canvas.getContext('2d');
  g.clearRect(0,0,canvas.width,canvas.height);g.save();
  const scale=Math.min(canvas.width/220,canvas.height/165);
  g.translate(canvas.width/2,canvas.height/2+2+Math.sin(time/650)*2*scale);g.scale(scale,scale);
  const ink=rgb(0x1085),paper=rgb(0xff7a),mint=rgb(0x87b6),peach=rgb(0xfdaf),lilac=rgb(0xc5ff),blush=custom?quantize(accent):rgb(0xfbb2);
  const skin=custom?quantize(body):[paper,mint,lilac][base];
  const ellipse=(x,y,rx,ry,c)=>{g.fillStyle=c;g.beginPath();g.ellipse(x,y,rx,ry,0,0,Math.PI*2);g.fill();};
  const rect=(x,y,w,h,r,c)=>{g.fillStyle=c;g.beginPath();g.roundRect(x,y,w,h,r);g.fill();};
  const tri=(x,y,x2,y2,x3,y3,c)=>{g.fillStyle=c;g.beginPath();g.moveTo(x,y);g.lineTo(x2,y2);g.lineTo(x3,y3);g.closePath();g.fill();};
  const line=(x,y,x2,y2,c)=>{g.strokeStyle=c;g.lineWidth=1.4;g.beginPath();g.moveTo(x,y);g.lineTo(x2,y2);g.stroke();};
  if(mood===4)g.translate(Math.sin(time/65)*4,Math.cos(time/90)*4);
  if(field===2){for(let i=0;i<4;i++){const x=-82+(Math.floor(time/25)+i*47)%164;line(x,45,Math.min(87,x+9),45,'#344557');}}
  ellipse(0,46,49,6,rgb(0x18e7));
  if(base===0){
    tri(-44,-16,-44,-52,-12,-28,skin);tri(44,-16,44,-52,12,-28,skin);
    tri(-38,-27,-38,-42,-25,-29,blush);tri(38,-27,38,-42,25,-29,blush);
    ellipse(0,0,52,42,skin);ellipse(-26,36,17,10,skin);ellipse(26,36,17,10,skin);
  }else if(base===1){
    tri(-40,15,-72,-16,-53,33,rgb(0x35d0));tri(40,15,72,-16,53,33,rgb(0x35d0));
    tri(-32,-24,-30,-55,-12,-31,peach);tri(32,-24,30,-55,12,-31,peach);
    ellipse(0,1,51,42,skin);ellipse(0,27,28,17,rgb(0xdff6));ellipse(-31,37,16,9,skin);ellipse(31,37,16,9,skin);
  }else{
    rect(-57,-13,12,31,5,rgb(0x8bdb));rect(45,-13,12,31,5,rgb(0x8bdb));
    rect(-2,-52,4,16,0,skin);ellipse(0,-55,7,7,peach);rect(-48,-36,96,77,22,skin);rect(-39,-25,78,49,16,ink);
    ellipse(-9,33,3,3,mint);ellipse(0,33,3,3,peach);ellipse(9,33,3,3,blush);
  }
  const eyeInk=custom?quantize(eye):base===2?mint:ink;
  const gaze=mood===4?Math.sin(time/100)*5:Math.sin(time/2100)*3;
  for(const side of [-1,1]){
    const x=side*21+gaze,y=-4;
    if(mood===3){ellipse(x-4,y-3,6,6,blush);ellipse(x+4,y-3,6,6,blush);tri(x-10,y,x+10,y,x,y+12,blush);}
    else if(mood===2||(time%4100<130&&mood!==4)){rect(x-9,y,18,4,2,eyeInk);}
    else if(mood===4||eyes===0||(eyes===5&&side<0)){ellipse(x,y,8,mood===4?16:12,eyeInk);ellipse(x-2,y-5,3,3,'white');}
    else if(eyes===1){tri(x,y-14,x-10,y,x+10,y,eyeInk);tri(x-10,y,x+10,y,x,y+14,eyeInk);ellipse(x,y,3,3,'white');}
    else if(eyes===2){rect(x-10,y-1,20,5,2,eyeInk);ellipse(x,y+5,5,4,eyeInk);}
    else if(eyes===3){ellipse(x,y,10,12,eyeInk);ellipse(x,y,3,10,base===2?paper:ink);ellipse(x-4,y-5,2,2,'white');}
    else if(eyes===4){rect(x-9,y-10,18,20,3,eyeInk);rect(x-5,y-6,5,5,0,'white');}
    else{line(x-9,y+2,x,y-3,eyeInk);line(x,y-3,x+9,y+2,eyeInk);}
  }
  if(base!==2){ellipse(-36,12,8,4,blush);ellipse(36,12,8,4,blush);}
  if(mood===4){ellipse(0,22,7,10,eyeInk);tri(58,-26,64,-11,52,-11,rgb(0x55ff));}
  else{line(-8,17,-4,21,eyeInk);line(-4,21,0,18,eyeInk);line(0,18,4,21,eyeInk);line(4,21,8,17,eyeInk);}
  const gy=mood===3||mood===4?-25:-4;
  if(eyewear===1){
    for(const side of [-1,1])for(let a=0;a<16;a++){const angle=a*2*Math.PI/16;ellipse(side*21+Math.trunc(Math.cos(angle)*13),gy+Math.trunc(Math.sin(angle)*14),1,1,blush);}
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
  g.restore();
}
