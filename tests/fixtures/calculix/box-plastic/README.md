# box-plastic: past yield, against a known answer

The 10 x 20 x 30 mm box, symmetry planes as box-setup, Pulled moved 0.5
mm (a stretch of 1.05, true strain ln 1.05 = 0.048790), in steel
measured by TenSense (`materials/`, as TenSense 0.3.0 wrote them: E =
199.9 GPa from the card, nu = 0.3 given in the study, the *PLASTIC block
of true stress against true plastic strain). Large displacements on by
themselves (a plastic material), 50 increments of 0.02. Written whole by
GridForge (`gf-study deck tension.json ...box.step --size 5 --order 2`),
solved by ccx 2.23 on 2026-11-17 with no warning.

## What is known, and checked (test_mesh_to_ccx)

- Uniaxial, the same at every node: sxx within 1e-3 MPa everywhere, the
  other components below 0.05 MPa.
- The stretch: ux = 0.05 x at every node, to the .frd's digits.
- The measured curve honoured: at each node's plastic strain (PE), the
  stress is TenSense's curve's, to 0.01 MPa (432.635 at 0.046725).
- Against a hand answer: ccx's large-strain plasticity for isotropic
  elasticity is Simo's (the ccx manual's incremental plasticity section),
  hyperelastic with yield on the Kirchhoff stress. Solved by hand for
  this uniaxial state (the lateral stress zero, the yield condition on the
  curve): Cauchy sxx = 432.1208 MPa at a plastic strain of 0.046629. ccx
  gives 432.635 here and converges, as its increments shrink, to 432.603
  (1 increment: 434.158; 50: 432.635; 200: 432.610; 1000: 432.603; tried
  2026-11-17). The last 0.11 % between ccx's converged value and the hand
  answer is NOT explained yet: ccx's stress sits on the curve itself (as
  a Cauchy stress), where Simo's model puts the Kirchhoff stress on it
  (0.09 % apart here), and the plastic strains differ by 7e-5. Held to 0.2
  % until ccx's own formulation is read.

## Why the increments matter

In one increment ccx gives 434.158 MPa, 0.36 % above its converged value:
large plastic increments are integrated less exactly. So a study gives
its increments (`"increments"` in a step, study spec 4.5), and GridForge
writes them with room for ccx to take them (`INC`).
