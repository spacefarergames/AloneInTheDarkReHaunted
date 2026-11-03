"""Extract this game's original embedded files without executing the release EXE.
Reference files supply candidate lengths, verified against the EXE's adjacent
compiled sizeof constants. Payload bytes always come from the release EXE.
"""
import argparse, hashlib, json, re, struct
from pathlib import Path

def extract(executable, reference, output, registry):
    data=executable.read_bytes()
    pe=struct.unpack_from('<I',data,0x3c)[0]
    if data[:2]!=b'MZ' or data[pe:pe+4]!=b'PE\0\0': raise ValueError('Not a PE image')
    optional=pe+24
    if struct.unpack_from('<H',data,optional)[0]!=0x20b: raise ValueError('Expected PE32+')
    base=struct.unpack_from('<Q',data,optional+24)[0]
    section_start=optional+struct.unpack_from('<H',data,pe+20)[0]
    sections=[]
    for index in range(struct.unpack_from('<H',data,pe+6)[0]):
        start=section_start+index*40
        _,rva,size,raw=struct.unpack_from('<IIII',data,start+8)
        sections.append((base+rva,size,raw))
    def offset(address):
        for va,size,raw in sections:
            if va<=address<va+size: return raw+address-va
        raise ValueError('Pointer is outside file-backed sections')
    def address(position):
        for va,size,raw in sections:
            if raw<=position<raw+size: return va+position-raw
        raise ValueError('Offset is outside sections')
    names=re.findall(r'\{\s*"([^"/]+)"\s*,\s*embdata_',registry.read_text(encoding='utf-8-sig'))
    anchor='LISTBOD2.PAK'; anchor_index=names.index(anchor)
    string_offset=data.index(anchor.encode()+b'\0')
    needle=struct.pack('<Q',address(string_offset)); candidates=[]
    position=data.find(needle)
    while position>=0:
        candidates.append(position-anchor_index*24); position=data.find(needle,position+1)
    verified=[]; errors=[]
    for table in candidates:
        try:
            entries=[]
            for index,name in enumerate(names):
                name_va,payload_va,_=struct.unpack_from('<QQQ',data,table+index*24)
                start=offset(name_va); end=data.index(0,start)
                if data[start:end].decode('ascii')!=name: raise ValueError('Registry mismatch')
                if (reference/name).exists(): length=(reference/name).stat().st_size
                else:
                    symbol='embdata_'+name.replace('.','_')
                    generated=registry.parent/('embedded_'+name.replace('.','_')+'.cpp')
                    with generated.open('rb') as source:
                        source.seek(max(0,generated.stat().st_size-4096)); tail=source.read().decode('ascii')
                    length=int(re.search(re.escape(symbol)+r'_size\s*=\s*(\d+)ULL',tail)[1])
                start=offset(payload_va)
                sizeof_offset=(start+length+7)&~7
                if struct.unpack_from('<Q',data,sizeof_offset)[0]!=length: raise ValueError('Embedded size verification failed for '+name)
                payload=data[start:start+length]
                if len(payload)!=length: raise ValueError('Truncated embedded file')
                entries.append((name,payload))
            verified.append(entries)
        except (ValueError,UnicodeError,struct.error,FileNotFoundError) as error: errors.append(str(error))
    if len(verified)!=1: raise ValueError('Could not identify exactly one verified AITD1 embedded registry: '+repr(errors))
    output.mkdir(parents=True,exist_ok=True); files=[]
    for name,payload in verified[0]:
        (output/name).write_bytes(payload)
        files.append(dict(path=name,length=len(payload),sha256=hashlib.sha256(payload).hexdigest(),reference_matches=payload==(reference/name).read_bytes() if (reference/name).exists() else None))
    manifest=dict(source=str(executable.resolve()),source_sha256=hashlib.sha256(data).hexdigest(),release=(executable.parent/'version.txt').read_text().strip(),files=files)
    (output/'release-provenance.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print('FITD_RELEASE_DATA_PASS:',len(files),'original files extracted from',manifest['release'],';',sum(f['length'] for f in files),'bytes')

if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('executable',type=Path); parser.add_argument('--reference',required=True,type=Path); parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args(); extract(args.executable,args.reference,args.output,Path(__file__).resolve().parents[2]/'FitdLib/embedded/embeddedData.cpp')
