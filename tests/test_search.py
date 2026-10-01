import json
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class SearchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        (self.root / 'a.md').write_text('Reliable queues retry failed jobs. Queues use leases.')
        (self.root / 'b.txt').write_text('Inventory transactions prevent overselling.')
        (self.root / 'c.md').write_text('Queues support inventory jobs.')

    def search(self, *words):
        result = subprocess.run([str(ROOT / 'build/seekline'), str(self.root), *words], capture_output=True, text=True, check=True)
        return json.loads(result.stdout)

    def test_and_queries_and_case(self):
        data = self.search('QUEUES', 'retry')
        self.assertEqual([hit['path'] for hit in data['matches']], ['a.md'])
        self.assertEqual(data['documents'], 3)
        self.assertEqual(self.search('unknown')['matches'], [])

    def test_repeated_terms_do_not_change_scores(self):
        self.assertEqual(self.search('queues'), self.search('queues', 'queues'))

    def test_binary_oversized_and_symlinks_are_skipped(self):
        (self.root / 'binary.txt').write_bytes(b'queues\x00retry')
        (self.root / 'large.txt').write_bytes(b'q' * (1024 * 1024 + 1))
        (self.root / 'link.txt').symlink_to(self.root / 'a.md')
        data = self.search('retry')
        self.assertEqual(data['documents'], 3)
        self.assertEqual(data['skipped'], 3)

    def test_bm25_rewards_relevant_document(self):
        (self.root / 'short.txt').write_text('rare rare rare')
        (self.root / 'long.txt').write_text('rare ' + 'filler ' * 500)
        matches = self.search('rare')['matches']
        self.assertEqual(matches[0]['path'], 'short.txt')
        self.assertGreater(matches[0]['score'], matches[1]['score'])

    def test_control_characters_in_names_are_json_escaped(self):
        (self.root / 'odd\nname.txt').write_text('distinctive')
        self.assertEqual(self.search('distinctive')['matches'][0]['path'], 'odd\nname.txt')

    def test_empty_query_fails(self):
        result = subprocess.run([str(ROOT / 'build/seekline'), str(self.root), '!!!'], capture_output=True)
        self.assertEqual(result.returncode, 2)

if __name__ == '__main__':
    unittest.main()
