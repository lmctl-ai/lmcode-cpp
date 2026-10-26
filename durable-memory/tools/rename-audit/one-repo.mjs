import fs from 'node:fs';import path from 'node:path';
const [repo,root,stdPath,sweepPath]=process.argv.slice(2);const std=JSON.parse(fs.readFileSync(stdPath,'utf8'));
const stdFns=new Set(Object.keys(std.functions||{}));const stdTypes=std.types||{};
const omit=new Set(['.git','node_modules','build','dist','target','coverage']);const textExt=new Set(['.md','.markdown','.mdx','.rst','.adoc','.txt','.html','.json','.yaml','.yml','.toml','.ts','.js','.mjs','.sh','.lms','.lm','.dsl','.cpp','.hpp','.h','.rs','.go','.cs','.cmake','.xml']);
function walk(d,rel='',out=[]){for(const e of fs.readdirSync(d,{withFileTypes:true})){if(omit.has(e.name))continue;const rr=path.join(rel,e.name),p=path.join(d,e.name);if(e.isDirectory())walk(p,rr,out);else if(e.isFile())out.push([p,rr]);}return out;}
let refs=[];let schema=[];let pins=new Set();
for(const [p,rel] of walk(root)){if(!textExt.has(path.extname(rel).toLowerCase()))continue;let t;try{t=fs.readFileSync(p,'utf8')}catch{continue;}
 const hits=[...t.matchAll(/cpp-dsl-sdk/gi)].length;if(hits)refs.push({path:rel,hits,urls:[...t.matchAll(/https?:\/\/github\.com\/(?:lmctl-ai|lmctlhq)\/cpp-dsl-sdk[^\s"')>]*/gi)].map(m=>m[0]),ci:/\.github\/workflows\//.test(rel)});
 if(path.extname(rel).toLowerCase()==='.json'&&/(schema|dsl)/i.test(rel)){try{const j=JSON.parse(t);let dup=[];for(const f of Object.keys(j.functions||{}))if(stdFns.has(f))dup.push('fn '+f);for(const [type,decl] of Object.entries(j.types||{})){if(stdTypes[type]){for(const method of Object.keys(decl.methods||{}))if(stdTypes[type].methods?.[method])dup.push('method '+type+'.'+method);}}if(dup.length)schema.push({path:rel,duplicates:[...new Set(dup)]});}catch{}}
 if(rel.startsWith('.github/workflows/')){for(const m of t.matchAll(/@lmctl-ai\/lmctl@(?:\$\{[^}]+\}|[0-9]+\.[0-9]+\.[0-9]+|latest)/g))pins.add(m[0]);for(const m of t.matchAll(/(?:LMCTL_VERSION|LMSCRIPT_VERSION):\s*([0-9]+\.[0-9]+\.[0-9]+)/g))pins.add(m[1]);}
}
const sw=JSON.parse(fs.readFileSync(sweepPath,'utf8'));const sr=sw.repositories?.[0];const blocks=sr?.files?.reduce((n,f)=>n+(f.blocks||0),0)??0;const ambiguous=sr?.files?.reduce((n,f)=>n+(f.ambiguous?.length||0),0)??0;const diagnostics=sr?.files?.flatMap(f=>(f.diagnostics||[]).map(d=>({path:f.path,message:d.message})))??[];
console.log(JSON.stringify({repo,blocks,ambiguous,diagnostics,oldNameFiles:refs.length,oldNameOccurrences:refs.reduce((n,x)=>n+x.hits,0),oldNameRefs:refs,schemaDuplicates:schema,workflowPins:[...pins]}));
