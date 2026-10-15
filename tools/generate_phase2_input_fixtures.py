"""Justification & Paper Alignment
Choice: generate deterministic byte fixtures from the existing Python lab.
Classification: Engineering Decision — not paper-derived.
Rule: call the actual preparation/BYPASS implementations without changing
formulas or normalization. This is parity infrastructure, not new science.
"""
from pathlib import Path
import argparse, hashlib, importlib, json, platform, sys
import cv2
import numpy as np

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--lab-root',required=True,type=Path)
    parser.add_argument('--output',required=True,type=Path)
    args=parser.parse_args()
    sys.path.insert(0,str(args.lab_root.resolve()))
    preparation=importlib.import_module('src.enhancement.experiment_b')
    baselines=importlib.import_module('src.enhancement.baselines')
    enhancer=baselines.BypassThermalEnhancer()
    rng=np.random.default_rng(20261015)
    args.output.mkdir(parents=True,exist_ok=False)
    entries=[];manifest=[]
    for rows,cols in [(1,1),(1,17),(19,1),(31,33)]:
        for channels in [1,3,4]:
            name=f'input_{rows}x{cols}_c{channels}'
            shape=(rows,cols) if channels==1 else (rows,cols,channels)
            image=rng.integers(0,256,shape,dtype=np.uint8)
            before=image.copy();expected=preparation.prepare_thermal_uint8_plane(image)
            bypass=enhancer.enhance(expected).output_image
            assert np.array_equal(before,image) and np.array_equal(expected,bypass)
            assert not np.shares_memory(expected,image) and not np.shares_memory(expected,bypass)
            for suffix,array in [('input',image),('expected',expected)]:
                target=args.output/f'{name}_{suffix}.bin';target.write_bytes(array.tobytes())
                entries.append({'file':target.name,'sha256':hashlib.sha256(target.read_bytes()).hexdigest()})
            manifest.append(f'{name} {rows} {cols} {channels}\n')
    (args.output/'cases.txt').write_text(''.join(manifest))
    sources={str(Path(m.__file__).relative_to(args.lab_root.resolve())):hashlib.sha256(Path(m.__file__).read_bytes()).hexdigest() for m in [preparation,baselines]}
    (args.output/'provenance.json').write_text(json.dumps({'contract':'P2_THERMAL_INPUT_V1','cases':12,'seed':20261015,'python':platform.python_version(),'opencv':cv2.__version__,'numpy':np.__version__,'sources':sources,'files':entries,'interpretation':'synthetic numerical fixtures; not enhancement quality evidence'},indent=2))
    print('Generated 12 reference preparation/BYPASS cases.')
if __name__=='__main__':main()
