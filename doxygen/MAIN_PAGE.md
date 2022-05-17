This provides a basic source-code generated documentation for the core classes of the vxlnetwork-node.
Doxygen docs may look a bit overwhelming as it tries to document all the smaller pieces of code. For
this reason only the files from `vxlnetwork` directory were added to this. Some other
files were also excluded as the `EXCLUDE_PATTERN` configuration stated below.

    EXCLUDE_PATTERNS       = */vxlnetwork/*_test/* \
                             */vxlnetwork/test_common/* \
                             */vxlnetwork/boost/* \
                             */vxlnetwork/qt/* \
                             */vxlnetwork/vxlnetwork_wallet/*

