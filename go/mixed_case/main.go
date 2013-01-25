package main

import (
	"log"
	"os"
	"runtime"
	"strconv"
)

func worker() (in chan uint64, out chan []uint64) {
	in = make(chan uint64)
	out = make(chan []uint64)

	go func() {
		for {
			out <- factorize(<-in)
		}
	}()

	return in, out
}

func chan_link(in chan uint64) (out chan uint64) {
	out = make(chan uint64, 128)

	go func() {
		var x uint64
		for {
			x = <-in
			out <- x

			if x == 0 {
				break
			}
		}
	}()

	return out
}

func create_ring(ring_size uint64) (in, out chan uint64) {
	in = make(chan uint64, 128)

	out = in
	for i := uint64(0); i < ring_size; i++ {
		out = chan_link(out)
	}

	return
}

func master(results chan []uint64, ring_size, initial_token, repetitions uint64) {
	go func() {
		win, wout := worker()

		for i := uint64(0); i < repetitions; i++ {
			win <- 28350160440309881

			in, out := create_ring(ring_size)
			done := make(chan bool)
			go func() {
				for {
					n := <-out
					if n == 0 {
						done <- true
						break
					}
				}
			}()

			token := initial_token
			for {
				in <- uint64(token)
				if token == 0 {
					break
				}
				token--
			}
			<-done

			results <- <-wout
		}
	}()
}

func run_masters(results chan []uint64, num_rings, ring_size, inital_token, repetitions uint64) {
	for i := uint64(0); i < num_rings; i++ {
		master(results, ring_size, inital_token, repetitions)
	}
}

func check_result(res []uint64) bool {
	if len(res) != 2 {
		return false
	}

	if (res[0] == 86028157 && res[1] == 329545133) || (res[1] == 86028157 && res[0] == 329545133) {
		return true
	}

	return false
}

func main() {
	runtime.GOMAXPROCS(runtime.NumCPU()) // use all machine's CPUs

	if len(os.Args) != 5 {
		log.Fatalf("Usage: %s num_rings ring_size initial_token repetitions", os.Args[0])
	}

	num_rings, _ := strconv.ParseUint(os.Args[1], 10, 64)
	ring_size, _ := strconv.ParseUint(os.Args[2], 10, 64)
	initial_token, _ := strconv.ParseUint(os.Args[3], 10, 64)
	repetitions, _ := strconv.ParseUint(os.Args[4], 10, 64)

	results := make(chan []uint64, 32)
	run_masters(results, num_rings, ring_size, initial_token, repetitions)

	for i := uint64(0); i < num_rings*repetitions; i++ {
		res := <-results
		if !check_result(res) {
			log.Fatalf("Wrong factorization result: %v", res)
		}
	}
}
