import copy,unittest
from differential_check import compare

class PresentationBoundaryTest(unittest.TestCase):
    def traces(self):
        # Exact reviewed E-3 Settings and E-5 title-menu snapshots; include a gameplay control.
        a={'E-3':[(i,'key',{'gram':'old','state':'stable','audio':'stable','save':'stable'},'unchanged') for i in range(5,11)],'E-5':[(i,'key',{'gram':'old','state':'stable','audio':'stable','save':'stable'},'unchanged') for i in [5,6]],'A-gameplay':[(0,'key',{'gram':'old','state':'stable','audio':'stable','save':'stable'},'unchanged')]}
        b=copy.deepcopy(a)
        for walk in ['E-3','E-5']:
            for row in b[walk]:row[2]['gram']='new'
        return a,b
    def test_exact_reviewed_menu_pixels(self):
        a,b=self.traces();self.assertEqual(len(compare(a,b)),8)
    def test_gameplay_save_audio_changes_fail_even_on_allowed_menu_steps(self):
        for field in ['state','save','audio']:
            a,b=self.traces();b['E-3'][0][2][field]='changed'
            with self.subTest(field=field),self.assertRaises(ValueError):compare(a,b)
    def test_unrelated_pixels_fail(self):
        a,b=self.traces();b['A-gameplay'][0][2]['gram']='changed'
        with self.assertRaises(ValueError):compare(a,b)
    def test_missing_expected_changes_fail(self):
        a,b=self.traces();b['E-3'][0][2]['gram']='old'
        with self.assertRaises(ValueError):compare(a,b)
if __name__=='__main__':unittest.main()
