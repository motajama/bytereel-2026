import importlib.util,json,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('assembler',ROOT/'firmware/bytereel_2026/assembler.py');mod=importlib.util.module_from_spec(spec);sys.modules['assembler']=mod;spec.loader.exec_module(mod)
class AsmTests(unittest.TestCase):
 def test_examples_match_all_256_bytes(self):
  for row in json.loads((ROOT/'examples/manifest.json').read_text()):
   got=mod.assemble((ROOT/'examples'/row['source']).read_text(),row['bank'])
   self.assertEqual(got,(ROOT/'examples'/f"{row['name']}.bin").read_bytes(),row['name'])
 def test_labels_hex_org_db(self):
  b=mod.assemble('JMP end\nORG 04\nDB 0b10101010, %11110000\nend: HALT\n')
  self.assertEqual(b[:6],bytes([0x30,6,0,0,0xaa,0xf0]))
 def test_bad_bank_odd_target_overlap(self):
  for src,bank in [('FREQ 20','VIDEO'),('JMP 03','ANY'),('00: DB 01 02\n01: DB 03','ANY')]:
   with self.assertRaises(ValueError):mod.assemble(src,bank)
 def test_opcode_cases_cover_canonical_table(self):
  text=(ROOT/'firmware/bytereel_2026/vm.h').read_text().lower()
  implemented={int(x,16) for x in __import__('re').findall(r'case\s+0x([0-9a-f]{2})\s*:',text)}
  expected={row['opcode'] for row in mod.TABLE.values()}
  self.assertEqual(expected-implemented,set())
 def test_argument_validation(self):
  for src in ['COUNT 00','WAVE 06','GATE 02','PATTERN 09']:
   with self.assertRaises(ValueError):mod.assemble(src,'AUDIO' if src.startswith(('COUNT','WAVE','GATE')) else 'VIDEO')
if __name__=='__main__':unittest.main()
