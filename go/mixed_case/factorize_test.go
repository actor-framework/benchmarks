package main

import (
	"testing"
)

func isEqual(a, b []uint64) bool {
	if len(a) != len(b) {
		return false
	}

	for i := 0; i < len(a); i++ {
		if a[i] != b[i] {
			return false
		}
	}

	return true
}

func check(n uint64, expected []uint64, t *testing.T) {
	factors := factorize(n)

	if !isEqual(factors, expected) {
		t.Errorf("Expected factors: %v, got factors: %v", expected, factors)
	}
}

func TestSimpleFactorize(t *testing.T) {
	check(288, []uint64{2, 2, 2, 2, 2, 3, 3}, t)
	check(24, []uint64{2, 2, 2, 3}, t)
}
