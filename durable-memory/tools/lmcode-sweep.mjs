import fs from 'node:fs';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {TextDecoder} from 'node:util';
import {pathToFileURL} from 'node:url';
import {homedir} from 'node:os';
import {createHash} from 'node:crypto';

const root = path.resolve(process.env.LMCODE_SWEEP_ROOT || path.join(homedir(), 'repos', 'dsl'));
const mode = process.env.LMCODE_SWEEP_MODE || 'dry-run';
const filter = process.env.LMCODE_SWEEP_REPO || '';
const stageRoot = process.env.LMCODE_SWEEP_STAGE ? path.resolve(process.env.LMCODE_SWEEP_STAGE) : '';
const reportPath = process.env.LMCODE_SWEEP_REPORT || '';
const compilerPath = process.env.LMCODE_SWEEP_COMPILER || '';
const scratch = process.env.LMCODE_SWEEP_SCRATCH || '/tmp/claude-1000/scratch-lmcode';
const skip = new Set(['lmproxy', 'lmctl-mono', 'lmdb', 'lmcode-java']);
// Extensions that can carry a script with a `_dsl_`/`_lmcode_` raw block.
// `.lms` is the primary spelling; `.lm` is the legacy script spelling the
// compiler still imports ("Lead rule: .lm is not exempt"), and `.dsl` is used
// by e.g. lmcode-csharp. Anything else is documentation or not inventoried.
const SCRIPT_EXTENSIONS = ['.lms', '.lm', '.dsl'];
const DOC_EXTENSIONS = ['.md', '.markdown', '.mdx', '.rst', '.adoc'];
const decoder = new TextDecoder('utf-8', {fatal: true, ignoreBOM: true});
const cmp = (a, b) => a < b ? -1 : a > b ? 1 : 0;
const hash = b => createHash('sha256').update(b).digest('hex');
const within = (parent, child) => child === parent || child.startsWith(parent + path.sep);
function git(repo, args) {
  try { return execFileSync('git', ['-C', repo, ...args], {encoding:'utf8', maxBuffer:64*1024*1024, stdio:['ignore','pipe','pipe']}); }
  catch { return ''; }
}
function repos() {
  const candidates = fs.readdirSync(root, {withFileTypes:true}).filter(d => d.isDirectory() && !skip.has(d.name)).map(d => path.join(root,d.name));
  // lmvideo currently lives outside the DSL parent; retain the adjacent layout too.
  for (const p of [path.resolve(root,'../lmvideo'), path.resolve(root,'../lmctlhq/lmvideo')]) if (fs.existsSync(p)) candidates.push(p);
  const found = new Map();
  for (const p of candidates.sort(cmp)) {
    if (skip.has(path.basename(p)) || !fs.existsSync(path.join(p,'.git')) || fs.lstatSync(p).isSymbolicLink()) continue;
    const repo = fs.realpathSync(p), name = path.basename(repo);
    if (skip.has(name) || filter && filter !== name) continue;
    const remotes = git(repo,['config','--get-regexp','^remote\\..*\\.url$']).trim().split('\n').map(l => l.replace(/^\S+\s+/,''));
    const remote = remotes.find(r => /github\.com[/:](?:lmctl-ai|lmctlhq)\//i.test(r));
    if (remote) found.set(repo,{name,repo,remote});
  }
  return [...found.values()].sort((a,b) => cmp(a.name,b.name));
}
function listed(repo) { return [...new Set(git(repo,['ls-files','-z','--cached','--others','--exclude-standard']).split('\0').filter(Boolean))].sort(cmp); }
function eligible(rel) {
  if (rel.split('/').some(p => ['.git','node_modules','third_party','build','target','dist'].includes(p))) return false;
  const base=path.basename(rel), ext=path.extname(rel).toLowerCase();
  return SCRIPT_EXTENSIONS.includes(ext) || /^readme(?:\.[^.]+)?$/i.test(base) || DOC_EXTENSIONS.includes(ext) || ext==='.txt' && rel.split('/').some(p => ['doc','docs','documentation','durable-memory'].includes(p));
}
function lines(text) {
  const result=[]; let start=0; const re=/\r\n|\r|\n/g; let m;
  while ((m=re.exec(text))) { result.push({start,end:m.index,text:text.slice(start,m.index),after:re.lastIndex}); start=re.lastIndex; }
  result.push({start,end:text.length,text:text.slice(start),after:text.length}); return result;
}
function mentions(text) { return [...text.matchAll(/(?<![A-Za-z0-9_])_dsl_(?![A-Za-z0-9_])/g)].map(m=>m.index); }
function apply(text, replacements) {
  let out=text;
  for (const r of [...replacements].sort((a,b)=>b.offset-a.offset)) out=out.slice(0,r.offset)+r.replacement+out.slice(r.offset+r.length);
  return out;
}
function scan(text, Lexer) {
  let tokens; const lexer=new Lexer(text);
  try { tokens=lexer.tokenize(); } catch(e) { return {replacements:[],blocks:0,diagnostics:[...lexer.diagnostics,{message:String(e.message||e)}]}; }
  const pending=[], pairs=[]; const physical=lines(text);
  for (const t of tokens) {
    if (t.type==='MARKER') pending.push(t);
    if (t.type!=='SEMICOLON' || !pending.length) continue;
    const closers=physical.filter(l=>l.start>t.offset && l.start<t.endOffset).map(l=>({line:l,match:/^end[ \t]+(_[A-Za-z0-9]+_)[ \t]*$/.exec(l.text)})).filter(c=>c.match);
    for (const marker of pending) {
      const closer=closers.shift();
      pairs.push({marker,closer,valid:!!marker.block?.closed && closer?.match[1]===marker.value});
    }
    pending.length=0;
  }
  if (lexer.diagnostics.length || pending.length || pairs.some(p=>!p.valid)) return {replacements:[],blocks:0,diagnostics:lexer.diagnostics.length?lexer.diagnostics:[{message:'unpaired or ambiguous raw-block delimiters'}]};
  const replacements=[]; let blocks=0;
  for (const {marker,closer} of pairs) {
    if (marker.value!=='_dsl_') continue;
    replacements.push({offset:marker.offset,length:5,replacement:'_lmcode_',reason:'lexer-paired block opener'});
    replacements.push({offset:closer.line.start+closer.line.text.indexOf('_dsl_'),length:5,replacement:'_lmcode_',reason:'lexer-paired block closer'}); blocks++;
  }
  if (replacements.length) {
    const next=new Lexer(apply(text,replacements));
    try { next.tokenize(); } catch(e) { return {replacements:[],blocks:0,diagnostics:[{message:'rewritten lexer rejected source: '+e.message}]}; }
    if (next.diagnostics.length) return {replacements:[],blocks:0,diagnostics:next.diagnostics};
  }
  return {replacements,blocks,diagnostics:[]};
}
function region(text,start,end,indent=0) {
  let code='', map=[];
  for (const l of lines(text.slice(start,end))) {
    const remove=Math.min(indent, (/^[ \t]*/.exec(l.text)||[''])[0].length);
    const segment=text.slice(start+l.start+remove,start+l.after);
    for(let i=0;i<segment.length;i++) map.push(start+l.start+remove+i);
    code+=segment;
  }
  return {text:code,map};
}
function docRegions(text) {
  const ls=lines(text), result=[], covered=new Set();
  for(let i=0;i<ls.length;i++) {
    const open=/^( {0,3})(`{3,}|~{3,})\s*([^\s`]*)[^\r\n]*$/.exec(ls[i].text);
    if(!open) continue;
    let j=i+1; const close=new RegExp('^ {0,3}'+open[2][0]+'{'+open[2].length+',}\\s*$');
    while(j<ls.length && !close.test(ls[j].text)) j++;
    for(let k=i;k<=Math.min(j,ls.length-1);k++) covered.add(k);
    const lang=open[3].toLowerCase();
    if(['lms','lmscript','lmcode','lm','dsl','', 'text'].includes(lang)) result.push(region(text,ls[i].after,j<ls.length?ls[j].start:text.length,open[1].length));
    else if(['sh','bash','shell','console'].includes(lang)) {
      for(let k=i+1;k<j;k++) {
        const h=/<<-?\s*(?:'([A-Za-z_]\w*)'|"([A-Za-z_]\w*)"|([A-Za-z_]\w*))/.exec(ls[k].text);
        if(!h) continue;
        const label=h[1]||h[2]||h[3]; if(!/^(?:lms|lmscript|lmcode|lm|dsl)$/i.test(label) && !/\.(?:lms|lm|dsl)\b/i.test(ls[k].text)) continue;
        let e=k+1; while(e<j && ls[e].text!==label) e++;
        if(e<j) {result.push(region(text,ls[k].after,ls[e].start));k=e;}
      }
    }
    i=j;
  }
  for(let i=0;i<ls.length;i++) {
    if(covered.has(i) || !/^( {4}|\t)/.test(ls[i].text)) continue;
    const start=i, indent=ls[i].text.startsWith('\t')?1:4;
    while(i+1<ls.length && !covered.has(i+1) && (/^( {4}|\t)/.test(ls[i+1].text)||!ls[i+1].text.trim())) i++;
    result.push(region(text,ls[start].start,ls[i].after,indent));
  }
  return result;
}
function transform(text,isLms,Lexer) {
  const regions=isLms?[{text,map:null}]:docRegions(text); const replacements=[],diagnostics=[];let blocks=0;
  for(const r of regions) {
    if(!mentions(r.text).length) continue;
    const s=scan(r.text,Lexer);blocks+=s.blocks;
    for(const item of s.replacements) replacements.push({...item,offset:r.map?r.map[item.offset]:item.offset});
    for(const d of s.diagnostics) diagnostics.push({...d,scope:isLms?'script':'documentation code'});
  }
  const unique=[...new Map(replacements.map(r=>[r.offset,r])).values()].sort((a,b)=>a.offset-b.offset);
  const changed=new Set(unique.map(r=>r.offset)); const ls=lines(text);
  const ambiguous=mentions(text).filter(o=>!changed.has(o)).map(offset=>{
    const line=ls.findIndex(l=>offset>=l.start&&offset<=l.end);
    return {offset,line:line+1,column:offset-ls[line].start+1,reason:'non-block text or unpaired delimiter; left unchanged',text:ls[line].text};
  });
  return {text:apply(text,unique),replacements:unique,blocks,diagnostics,ambiguous};
}
function selfTest(Lexer) {
  const tests=[
    ['p=_dsl_;\nend _dsl_\n','p=_lmcode_;\nend _lmcode_\n',true],
    ['p=_dsl_;\r\nend _dsl_  \r\n','p=_lmcode_;\r\nend _lmcode_  \r\n',true],
    ['\ufeffp=_dsl_;\rend _dsl_\r',null,true],
    ['p=_dsl_;\rend _dsl_\r','p=_lmcode_;\rend _lmcode_\r',true],
    ['# _dsl_\ns="_dsl_";\nf=f"_dsl_";\n',null,true],
    ['a=_csv_;\n_dsl_\nend _csv_\n',null,true],
    ['a=_json_;\n{"x":"_dsl_"}\nend _json_\n',null,true],
    ['a=_lmcode_;\nend _lmcode_\n',null,true],
    ['x=[_csv_,_dsl_];\na\nend _csv_\nlet s="_dsl_";\nend _dsl_\n','x=[_csv_,_lmcode_];\na\nend _csv_\nlet s="_dsl_";\nend _lmcode_\n',true],
    ['p=_dsl_\nend _dsl_\n',null,true],
    ['p=_dsl_;\n end _dsl_\n',null,true],
    ['p=_dsl_;\nend _json_\n',null,true],
    ['p=_dsl_;\nend _dsl_;\n',null,true],
    ['p=_dsl_;\n${"}"}\nend _dsl_\n','p=_lmcode_;\n${"}"}\nend _lmcode_\n',true],
    ['Mention `_dsl_`, --dsl-schema, .dsl.json and dslvm.\n```lms\np=_dsl_;\nend _dsl_\n```\n','Mention `_dsl_`, --dsl-schema, .dsl.json and dslvm.\n```lms\np=_lmcode_;\nend _lmcode_\n```\n',false],
    ['    p=_dsl_;\n    end _dsl_\n','    p=_lmcode_;\n    end _lmcode_\n',false],
    ["```sh\ncat > a.lms <<'LMS'\np=_dsl_;\nend _dsl_\nLMS\n```\n","```sh\ncat > a.lms <<'LMS'\np=_lmcode_;\nend _lmcode_\nLMS\n```\n",false],
    ['```json\n{"x":"_dsl_"}\n```\n',null,false],
  ];
  for(const [source,expected,lms] of tests) {
    const actual=transform(source,lms,Lexer).text;
    if(actual!==(expected??source)) throw Error('transformation regression: '+JSON.stringify({source,actual,expected:expected??source}));
  }
  if(decoder.decode(Buffer.from('\ufeffx'))!=='\ufeffx') throw Error('BOM preservation failed');
  return tests.length+1;
}
async function main() {
  if(mode==='apply' && stageRoot) throw Error('--stage is for dry-run only; it cannot be combined with --apply');
  if(!compilerPath || !fs.existsSync(compilerPath)) throw Error('Set LMSCRIPT_LEXER or --lexer to the built origin/main lmscript/dist/lmscript.js');
  const {Lexer}=await import(pathToFileURL(path.resolve(compilerPath)).href);
  if(!Lexer) throw Error('compiler module does not export Lexer');
  selfTest(Lexer); // Fail closed if this compiler has incompatible lexer behavior.
  if(process.env.LMCODE_SWEEP_SELFTEST==='1') {console.log(JSON.stringify({self_test:selfTest(Lexer)}));return;}
  if(reportPath && !within(scratch,path.resolve(reportPath))) throw Error('--report-json must be under '+scratch);
  if(stageRoot) {
    let ancestor=stageRoot; while(!fs.existsSync(ancestor)) ancestor=path.dirname(ancestor);
    if(!within(fs.realpathSync(scratch),fs.realpathSync(ancestor))) throw Error('--stage resolves outside the scratch directory');
  }
  const repositories=repos();
  if(stageRoot && !within(scratch,stageRoot)) throw Error('--stage must be under '+scratch);
  const result={mode,root,compiler:compilerPath,repositories:[],totals:{files:0,changed_files:0,examples:0,changed_examples:0,blocks:0,replacements:0,ambiguous:0,diagnostics:0}};
  for(const info of repositories) {
    const files=listed(info.repo), rr={...info,files:[],json_dependencies:[],skipped_extensions:{}};
    const skippedExts=new Map(); let skippedMarkerMentions=0;
    for(const rel of files) {
      const file=path.join(info.repo,rel);
      if(!eligible(rel)) {
        const ext=path.extname(rel).toLowerCase()||'(none)';
        skippedExts.set(ext,(skippedExts.get(ext)||0)+1);
        // Loud, not silent: a skipped file that still mentions a block marker may be
        // a script under an extension the allowlist does not cover yet.
        try {
          const st=fs.lstatSync(file);
          if(st.isFile()&&st.size<=1024*1024) {
            const bytes=fs.readFileSync(file);
            if(bytes.includes('_dsl_')||bytes.includes('_lmcode_')) {
              skippedMarkerMentions++;
              console.error(`WARNING: ${info.name}/${rel}: skipped (extension '${ext}' is not a covered script/documentation type) but mentions _dsl_/_lmcode_; check whether the allowlist needs it`);
            }
          }
        } catch {}
        continue;
      }
      if(!fs.existsSync(file)||!fs.lstatSync(file).isFile()) continue;
      let bytes,text;
      try {bytes=fs.readFileSync(file);text=decoder.decode(bytes);}catch(e){rr.files.push({path:rel,error:String(e.message),replacements:[]});continue;}
      const isScript=SCRIPT_EXTENSIONS.includes(path.extname(rel).toLowerCase()), t=transform(text,isScript,Lexer);
      const snippets=[];
      if(!isScript && stageRoot) {
        let index=0;
        for(const r of docRegions(text)) {
          const found=scan(r.text,Lexer);if(!found.replacements.length)continue;
          const name='.lmcode-sweep-snippet-'+path.basename(rel).replace(/[^A-Za-z0-9_.-]/g,'_')+'-'+(++index);
          const target=path.join(stageRoot,info.name,path.dirname(rel),name+'.lms');
          const baseline=path.join(stageRoot,info.name,path.dirname(rel),name+'.baseline');
          fs.mkdirSync(path.dirname(target),{recursive:true});fs.writeFileSync(target,apply(r.text,found.replacements));fs.writeFileSync(baseline,r.text);
          snippets.push({file:path.relative(path.join(stageRoot,info.name),target),baseline:path.relative(path.join(stageRoot,info.name),baseline),source_document:rel,source_line:lines(text).findIndex(l=>r.map[0]>=l.start&&r.map[0]<=l.end)+1});
        }
      }
      rr.files.push({path:rel,kind:isScript?'script':'documentation',source_sha256:hash(bytes),planned_sha256:hash(Buffer.from(t.text)),blocks:t.blocks,replacements:t.replacements,diagnostics:t.diagnostics,ambiguous:t.ambiguous,snippets});
      result.totals.files++;result.totals.examples+=Number(isScript);result.totals.changed_files+=Number(t.replacements.length>0);result.totals.changed_examples+=Number(isScript&&t.replacements.length>0);
      for(const key of ['blocks','replacements','ambiguous','diagnostics']) result.totals[key]+=Array.isArray(t[key])?t[key].length:t[key];
      if(mode==='apply' && t.replacements.length) fs.writeFileSync(file,Buffer.from(t.text));
      if(stageRoot){const target=path.join(stageRoot,info.name,rel);fs.mkdirSync(path.dirname(target),{recursive:true});fs.writeFileSync(target,Buffer.from(t.text));}
    }
    const json=new Set(files.filter(p=>p.endsWith('.json')&&!p.split('/').some(n=>['node_modules','third_party','build','target','dist'].includes(n))));
    const schemaDir=path.join(info.repo,'schema');if(fs.existsSync(schemaDir)) for(const name of fs.readdirSync(schemaDir)) if(name.endsWith('.json')) json.add('schema/'+name);
    rr.json_dependencies=[...json].sort(cmp);
    rr.skipped_extensions=Object.fromEntries([...skippedExts.entries()].sort((a,b)=>cmp(a[0],b[0])));
    rr.skipped_marker_mentions=skippedMarkerMentions;
    result.totals.skipped_files=(result.totals.skipped_files||0)+[...skippedExts.values()].reduce((a,b)=>a+b,0);
    result.totals.skipped_marker_mentions=(result.totals.skipped_marker_mentions||0)+skippedMarkerMentions;
    if(skippedMarkerMentions) console.error(`WARNING: ${info.name}: ${skippedMarkerMentions} skipped file(s) mention _dsl_/_lmcode_ markers; see skipped_extensions in the report`);
    if(stageRoot) for(const rel of rr.json_dependencies){const file=path.join(info.repo,rel);if(!fs.existsSync(file)||!fs.lstatSync(file).isFile())continue;const target=path.join(stageRoot,info.name,rel);fs.mkdirSync(path.dirname(target),{recursive:true});fs.copyFileSync(file,target);}
    result.repositories.push(rr);
  }
  const output=JSON.stringify(result,null,2)+'\n';
  if(reportPath){if(!within(scratch,path.resolve(reportPath)))throw Error('--report-json must be under '+scratch);fs.mkdirSync(path.dirname(reportPath),{recursive:true});fs.writeFileSync(reportPath,output);console.log(JSON.stringify(result.totals));}else process.stdout.write(output);
  if(result.totals.diagnostics) process.exitCode=1;
}
main().catch(e=>{console.error(e.stack||e);process.exitCode=2;});
