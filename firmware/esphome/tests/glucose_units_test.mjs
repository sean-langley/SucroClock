import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
const header=fs.readFileSync(new URL('../components/clock_setup/dashboard.h',import.meta.url),'utf8');
const code=header.split('// BEGIN UNIT CONVERSION HELPERS')[1].split('// END UNIT CONVERSION HELPERS')[0];
const context={};vm.runInNewContext(code+'\nthis.units=GlucoseUnits;',context);const u=context.units;
assert.equal(u.format(70,'mmol/L'),'3.9');assert.equal(u.format(180,'mmol/L'),'10.0');assert.equal(u.format(250,'mmol/L'),'13.9');
assert.equal(u.format(10,'mmol/L',true),'0.56');
for(let original=20;original<=600;original++){
 let stored=original;
 for(let cycle=0;cycle<50;cycle++){
  stored=u.read(Number(u.format(stored,'mmol/L')),'mmol/L',stored);
  stored=u.read(Number(u.format(stored,'mg/dL')),'mg/dL',stored);
 }
 assert.equal(stored,original,'Unit switching must not shift an unedited threshold');
}
for(const original of [.1,.2,1,10,10.1,49.9,50]){
 assert.equal(u.read(Number(u.format(original,'mmol/L',true)),'mmol/L',original,true),original);
}
assert.equal(u.read(4.2,'mmol/L',70),76);assert.equal(u.read(11,'mmol/L',180),198);
assert.equal(u.read(.5,'mmol/L',10,true),9);assert.equal(u.read(2.78,'mmol/L',10,true),50);
assert.equal(u.read(80,'mg/dL',70),80);assert(Number.isNaN(u.read(NaN,'mmol/L',70)));
// Exercise the real form functions with a minimal field model; no copied UI logic.
const ids=['styleLow','styleTarget','styleHigh','styleBlend','rangeUnitsHint','blendLabel','lowColorLabel','targetColorLabel','highColorLabel','veryHighColorLabel','styleMode','blendBox','blendHint'];
const fields=Object.fromEntries(ids.map(id=>[id,{value:'',hidden:false,textContent:'',get valueAsNumber(){return this.value===''?NaN:Number(this.value);}}]));
const formContext={$:id=>fields[id],setValue:(id,value)=>{fields[id].value=String(value);},styleUnits:'mg/dL',rangeDraft:{low:71,target_high:180,high:250,blend_width:.1}};
vm.createContext(formContext);vm.runInContext(code+header.slice(header.indexOf('function captureRanges()'),header.indexOf('function fillStyle(')),formContext);
formContext.renderRanges('mg/dL');fields.styleMode.value='smooth';formContext.rangeLabels();
assert.equal(fields.styleLow.value,'71');assert.equal(fields.styleLow.step,'1');
assert.equal(formContext.captureRanges(),true);formContext.renderRanges('mmol/L');formContext.rangeLabels();
assert.equal(fields.styleLow.value,'3.9');assert.equal(fields.styleTarget.value,'10.0');assert.equal(fields.styleBlend.value,'0.01');
assert.equal(fields.styleLow.step,'.1');assert.equal(fields.styleBlend.step,'.01');
assert.equal(fields.blendLabel.textContent,'Blend width (mmol/L)');assert(fields.lowColorLabel.textContent.includes('mmol/L'));
assert.equal(formContext.captureRanges(),true);assert.equal(formContext.rangeDraft.low,71);assert.equal(formContext.rangeDraft.blend_width,.1);
fields.styleLow.value='4.2';assert.equal(formContext.captureRanges(),true);assert.equal(formContext.rangeDraft.low,76);
formContext.renderRanges('mg/dL');assert.equal(fields.styleLow.value,'76');assert.equal(fields.styleBlend.value,'0.1');
fields.styleLow.value='';assert.equal(formContext.captureRanges(),false);assert.equal(formContext.rangeDraft.low,76);
console.log('Unit-aware range conversion, unchanged-save stability, edited values, blend precision, labels and incomplete-input tests passed.');
