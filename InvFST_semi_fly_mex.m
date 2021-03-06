% [Points] = InvFST_semi_fly_mex(FSTRep)
%
% Calculate the inverse spherical harmonic transform of a sampled
% band-limited function using S2Kit.
%
% FSTRep is a list of spherical harmonic expansion coefficients in the order
% described in the S2kit documentation. It contains bandwidth^2 elements.
%
% Points is a set of values for each (theta, phi) point on the grid
% generated by MakeFSTGrid(bandwidth). It contains 2*bandwidth x
% 2*bandwidth elements.
