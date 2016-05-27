include buildcfg.mk

######################################################################
# Build options

DIR		:= src
TESTDIR	:= test

######################################################################
# Build
all:
	@for dir in $(DIR); do		\
	make -C $$dir || exit $?;	\
	done

install:
	@for dir in $(DIR); do			\
	make -C $$dir install || exit $?;	\
	done

clean:
	@for dir in $(DIR); do			\
	make -C $$dir clean || exit $?;	\
	done

distclean:
	@for dir in $(DIR); do			\
	make -C $$dir distclean || exit $?;	\
	done

testall:
	@for dir in $(TESTDIR); do	\
	make -C $$dir || exit $?;	\
	done

testinstall:
	@for dir in $(TESTDIR); do		\
	make -C $$dir install || exit $?;	\
	done

testclean:
	@for dir in $(TESTDIR); do		\
	make -C $$dir clean || exit $?;	\
	done

testdistclean:
	@for dir in $(TESTDIR); do		\
	make -C $$dir distclean || exit $?;	\
	done
