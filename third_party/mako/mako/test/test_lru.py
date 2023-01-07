from mako.util import LRUCache


class item:
    def __init__(self, id_):
        self.id = id_

    def __str__(self):
        return "item id %d" % self.id


class LRUTest:
    def testlru(self):
        l = LRUCache(10, threshold=0.2)

        for id_ in range(1, 20):
            l[id_] = item(id_)

        # first couple of items should be gone
        assert 1 not in l
        assert 2 not in l

        # next batch over the threshold of 10 should be present
        for id_ in range(11, 20):
            assert id_ in l

        l[12]
        l[15]
        l[23] = item(23)
        l[24] = item(24)
        l[25] = item(25)
        l[26] = item(26)
        l[27] = item(27)

        assert 11 not in l
        assert 13 not in l

        for id_ in (25, 24, 23, 14, 12, 19, 18, 17, 16, 15):
            assert id_ in l
