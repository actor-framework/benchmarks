package main

func factorize(n uint64) []uint64 {
	result := []uint64{}

	if n <= 3 {
		return append(result, n)
	}

	var d uint64 = 2

	for d < n {
		if (n % d) == 0 {
			result = append(result, d)
			n /= d
		} else {
			if d == 2 {
				d = 3
			} else {
				d += 2
			}
		}
	}

	return append(result, d)
}
