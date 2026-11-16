// Derived stress quantities against hand answers. Each tensor's principal
// values are known by construction; von Mises follows from them.

#include "core/Stress.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <vector>

using namespace gf::core;

namespace {

int failures = 0;
std::vector<std::pair<const char *, std::function<void()>>> &cases()
{
    static std::vector<std::pair<const char *, std::function<void()>>> c;
    return c;
}
struct Register {
    Register(const char *n, std::function<void()> f) { cases().push_back({n, std::move(f)}); }
};
#define CASE(name)                                  \
    void name();                                    \
    static Register reg_##name(#name, name);        \
    void name()
#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
            ++failures;                                                                \
        }                                                                              \
    } while (0)

bool close(double a, double b) { return std::abs(a - b) <= 1e-9 * std::max(1.0, std::abs(b)); }
bool same(std::array<double, 3> a, std::array<double, 3> b)
{
    return close(a[0], b[0]) && close(a[1], b[1]) && close(a[2], b[2]);
}

} // namespace

CASE(uniaxial_tension_is_its_own_von_mises_and_first_principal)
{
    const SymmetricTensor s{0, 0, 210, 0, 0, 0};
    CHECK(close(vonMises(s), 210));
    CHECK(same(principalValues(s), {210, 0, 0}));
}

// Pure shear tau: principal +tau, 0, -tau; von Mises sqrt(3) tau.
CASE(pure_shear_in_each_plane_gives_plus_and_minus_tau)
{
    for (SymmetricTensor s : {SymmetricTensor{0, 0, 0, 100, 0, 0}, SymmetricTensor{0, 0, 0, 0, 100, 0},
                              SymmetricTensor{0, 0, 0, 0, 0, 100}}) {
        CHECK(close(vonMises(s), std::sqrt(3.0) * 100));
        CHECK(same(principalValues(s), {100, 0, -100}));
    }
}

CASE(hydrostatic_pressure_has_no_von_mises_stress)
{
    const SymmetricTensor s{-50, -50, -50, 0, 0, 0};
    CHECK(close(vonMises(s), 0));
    CHECK(same(principalValues(s), {-50, -50, -50}));
}

// xx = yy = 50, xy = 30: 50 +- 30 in the plane, 0 out of it.
// zx couples x and z: [[30, 40], [40, 0]] has 15 +- sqrt(15^2 + 40^2).
CASE(mixed_tensors_give_the_eigenvalues_known_by_hand)
{
    const SymmetricTensor a{50, 50, 0, 30, 0, 0};
    CHECK(same(principalValues(a), {80, 20, 0}));
    CHECK(close(vonMises(a), std::sqrt(5200.0)));
    const SymmetricTensor b{30, 0, 0, 0, 0, 40};
    const double r = std::sqrt(15.0 * 15.0 + 40.0 * 40.0);
    CHECK(same(principalValues(b), {15 + r, 0, 15 - r}));
    CHECK(close(vonMises(b), std::sqrt(5700.0)));
}

int main()
{
    for (const auto &[name, f] : cases()) {
        const int before = failures;
        f();
        std::printf("%s %s\n", failures == before ? "PASS" : "FAIL", name);
    }
    std::printf("%zu cases, %d checks failed\n", cases().size(), failures);
    return failures == 0 ? 0 : 1;
}
