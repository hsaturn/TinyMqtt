SUB=

all:runtests

tests:
	@set -e; \
	for i in $(SUB)*-tests/Makefile; do \
		echo '==== Making:' $$(dirname $$i); \
		$(MAKE) -C $$(dirname $$i) -j; \
	done
	$(MAKE) -C compile-test

valgrind:
	@set -e; \
	$(MAKE) tests; \
	for i in $(SUB)*-tests/Makefile; do \
		echo '==== Running:' $$(dirname $$i); \
		valgrind $$(dirname $$i)/$$(dirname $$i).out; \
	done

debugtest:
	@set -e; \
	$(MAKE) clean; \
	$(MAKE) -C debug-mode -j; \
	debug-mode/debug-tests.out

runtests:
	@set -e; \
	$(MAKE) tests; \
	for i in $(SUB)*-tests/Makefile; do \
		echo '==== Running:' $$(dirname $$i); \
		$$(dirname $$i)/$$(dirname $$i).out; \
	done

clean:
	@set -e; \
	for i in $(SUB)*-tests/Makefile; do \
		echo '==== Cleaning:' $$(dirname $$i); \
		$(MAKE) -C $$(dirname $$i) clean; \
	done
