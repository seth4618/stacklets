.global atomic_xchange

atomic_xchange:
	movq 	$1, 		%rax
	xchg 	%rax, 		(%rdi) # Exchange the current lock status with the lock (1)
	ret