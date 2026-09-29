// Build computer only. Preserve top-level names shared by separate inline scripts.
const terser = require('terser');
if (require('terser/package.json').version !== '5.44.0') throw Error('Terser 5.44.0 required');
let source='';process.stdin.setEncoding('utf8');process.stdin.on('data',s=>source+=s);
process.stdin.on('end',async()=>{try{const result=await terser.minify(source,{ecma:2020,compress:{passes:3},mangle:true});process.stdout.write(result.code)}catch(e){console.error(e);process.exitCode=1}});
