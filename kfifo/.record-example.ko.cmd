cmd_/workspace/studiocode/kfifo/record-example.ko := ld -r -m elf_x86_64  -z max-page-size=0x200000 -T ./scripts/module-common.lds  --build-id  -o /workspace/studiocode/kfifo/record-example.ko /workspace/studiocode/kfifo/record-example.o /workspace/studiocode/kfifo/record-example.mod.o ;  true