// Original Dash-Chan vector art; mirrors include/avatars.h at RGB565 precision.
export function quantize(hex){const c=parseInt(hex.replace('#',''),16);const r=c>>16,g=(c>>8)&255,b=c&255;return `rgb(${Math.round((r>>3)*255/31)},${Math.round((g>>2)*255/63)},${Math.round((b>>3)*255/31)})`;}
export function drawPet(canvas,{base=0,body='#fff3d2',accent='#ff88b6',accessory=0,mood=0,custom=false}={},time=0){
  const g=canvas.getContext('2d');g.clearRect(0,0,canvas.width,canvas.height);g.save();g.translate(160,canvas.height/2+2+Math.sin(time/650)*2);g.scale(1.45,1.45);
  const rgb=c=>`rgb(${Math.round((c>>11)*255/31)},${Math.round(((c>>5)&63)*255/63)},${Math.round((c&31)*255/31)})`;
  const ink=rgb(0x1085),paper=rgb(0xff7a),mint=rgb(0x87b6),peach=rgb(0xfdaf),lilac=rgb(0xc5ff),blush=custom?quantize(accent):rgb(0xfbb2);
  const skin=custom?quantize(body):[paper,mint,lilac][base];
  const ellipse=(x,y,rx,ry,c)=>{g.fillStyle=c;g.beginPath();g.ellipse(x,y,rx,ry,0,0,Math.PI*2);g.fill();};
  const rect=(x,y,w,h,r,c)=>{g.fillStyle=c;g.beginPath();g.roundRect(x,y,w,h,r);g.fill();};
  const tri=(x,y,x2,y2,x3,y3,c)=>{g.fillStyle=c;g.beginPath();g.moveTo(x,y);g.lineTo(x2,y2);g.lineTo(x3,y3);g.closePath();g.fill();};
  if(mood===4)g.translate(Math.sin(time/65)*3,Math.cos(time/90)*3);
  ellipse(0,46,49,6,'#182438');
  if(base===0){tri(-44,-16,-44,-52,-12,-28,skin);tri(44,-16,44,-52,12,-28,skin);tri(-38,-27,-38,-42,-25,-29,blush);tri(38,-27,38,-42,25,-29,blush);ellipse(0,0,52,42,skin);ellipse(-26,36,17,10,skin);ellipse(26,36,17,10,skin);}
  else if(base===1){tri(-40,15,-72,-16,-53,33,'#32b980');tri(40,15,72,-16,53,33,'#32b980');tri(-32,-24,-30,-55,-12,-31,peach);tri(32,-24,30,-55,12,-31,peach);ellipse(0,1,51,42,skin);ellipse(0,27,28,17,'#ddffb5');ellipse(-31,37,16,9,skin);ellipse(31,37,16,9,skin);}
  else {rect(-57,-13,12,31,5,'#8a79de');rect(45,-13,12,31,5,'#8a79de');rect(-2,-52,4,16,0,skin);ellipse(0,-55,7,7,peach);rect(-48,-36,96,77,22,skin);rect(-39,-25,78,49,16,ink);ellipse(-9,33,3,3,mint);ellipse(0,33,3,3,peach);ellipse(9,33,3,3,blush);}
  const eye=base===2?mint:ink,gaze=mood===4?Math.sin(time/100)*5:Math.sin(time/2100)*3;
  for(const x of [-21+gaze,21+gaze]){if(mood===3){ellipse(x-4,-7,6,6,blush);ellipse(x+4,-7,6,6,blush);tri(x-10,-4,x+10,-4,x,8,blush);}else if(time%4100<130&&mood!==4)rect(x-9,-4,18,4,2,eye);else{ellipse(x,-4,8,mood===4?16:12,eye);ellipse(x-2,-9,3,3,'white');}}
  if(base!==2){ellipse(-36,12,8,4,blush);ellipse(36,12,8,4,blush);}
  if(mood===4){ellipse(0,22,7,10,eye);tri(58,-26,64,-11,52,-11,'#55bfff');}else{g.strokeStyle=eye;g.lineWidth=1.4;g.beginPath();g.moveTo(-8,17);g.lineTo(-4,21);g.lineTo(0,18);g.lineTo(4,21);g.lineTo(8,17);g.stroke();}
  if(accessory===1){tri(-6,-34,-23,-44,-23,-24,blush);tri(6,-34,23,-44,23,-24,blush);ellipse(0,-34,6,6,blush);}
  else if(accessory===2){rect(-37,31,74,8,0,blush);rect(22,31,12,22,4,blush);}
  else if(accessory===3){rect(-22,-42,44,10,0,peach);tri(-22,-42,-22,-59,-8,-42,peach);tri(-10,-42,0,-61,10,-42,peach);tri(8,-42,22,-59,22,-42,peach);}
  g.restore();
}
