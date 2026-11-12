#include "../src/policy.hpp"
#include <cassert>
#include <cstdio>
using namespace no_fail;
int main() {
    for (auto g : {Gauge::hard, Gauge::exhard}) {
        Track t; t.reset(g);
        t.apply(g, -4900); assert(t.value == 100 && t.clear(g));
        t.apply(g, -99); assert(t.value == 1 && t.failed && !t.clear(g));
        t.apply(g, -1); assert(t.failed && !t.clear(g) && !t.red());
        t.apply(g, -5000); assert(t.value == -5000 && t.red());
        t.apply(g, 10000); assert(t.value == 5000 && !t.clear(g));
        t.reset(g); assert(t.clear(g));
        for (int i=0; i<100; ++i) t.apply(g, -5000);
        assert(t.value == -495000); // Cannot fit the old int16 graph API.
    }
    for (auto g : {Gauge::hard, Gauge::exhard}) for (int low : {-1, 0, 1, 49, 50, 99, 100}) {
        Track t; t.reset(g); t.apply(g, low - 5000);
        assert(t.value == low && t.failed == (low < 100));
        t.apply(g, 5000); assert(t.clear(g) == (low >= 100));
        // A stage carry must be observed before the first recovery judgement.
        t.reset(g); t.value = low; t.apply(g, 500);
        assert(t.failed == (low < 100));
    }
    for (auto g : {Gauge::normal, Gauge::easy, Gauge::assisted}) {
        Track t; t.reset(g); t.apply(g, -100000);
        assert(t.value == 100 && !t.red());
        t.apply(g, g == Gauge::assisted ? 2900 : 3900);
        assert(t.clear(g));
    }
    Player p; p.reset(Gauge::exhard, Gauge::assisted, true);
    assert(p.upload_clear() == Gauge::exhard);
    p.tracks[4].apply(Gauge::exhard, -5000);
    assert(p.display() == Gauge::hard && p.upload_clear() == Gauge::hard);
    p.tracks[3].apply(Gauge::hard, -6000);
    assert(p.display() == Gauge::assisted && !p.upload_clear());
    p.tracks[1].apply(Gauge::assisted, 1900);
    assert(p.upload_clear() == Gauge::assisted);
    p.tracks[0].apply(Gauge::normal, 2900);
    assert(p.upload_clear() == Gauge::normal);
    p.tracks[4].apply(Gauge::exhard, 5000);
    assert(p.upload_clear() == Gauge::normal);
    p.externally_blocked = true; assert(!p.upload_clear());
    p.reset(Gauge::hard, Gauge::assisted, false);
    p.tracks[3].apply(Gauge::hard, -6000);
    assert(p.display() == Gauge::hard && !p.upload_clear());
    p.reset(Gauge::easy, Gauge::normal, true);
    p.tracks[2].apply(Gauge::easy, 2900);
    assert(p.upload_clear() == Gauge::easy); // Never upgrade above selected.
    Player other; other.reset(Gauge::exhard, Gauge::assisted, true);
    assert(other.upload_clear() == Gauge::exhard);
    assert(graph_y(5000, 400, 200) == 200);
    assert(graph_y(0, 400, 200) == 400);
    assert(graph_y(-5000, 400, 200) == 600);
    assert(graph_y(-50000, 400, 200) == 2400);
    std::puts("PASS: signed gauge policy, permanent failure, automatic selection, upload policy, graph scale");
}
