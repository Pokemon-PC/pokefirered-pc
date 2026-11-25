	.section script_data, "aw", %progbits

	.align 2
gMysteryEventScriptCmdTable::
#ifdef PLATFORM_PC
	.8byte MEScrCmd_nop
	.8byte MEScrCmd_checkcompat
	.8byte MEScrCmd_end
	.8byte MEScrCmd_setmsg
	.8byte MEScrCmd_setstatus
	.8byte MEScrCmd_runscript
	.8byte MEScrCmd_initramscript
	.8byte MEScrCmd_setenigmaberry
	.8byte MEScrCmd_giveribbon
	.8byte MEScrCmd_givenationaldex
	.8byte MEScrCmd_addrareword
	.8byte MEScrCmd_setrecordmixinggift
	.8byte MEScrCmd_givepokemon
	.8byte MEScrCmd_addtrainer
	.8byte MEScrCmd_enableresetrtc
	.8byte MEScrCmd_checksum
	.8byte MEScrCmd_crc
#else
	.4byte MEScrCmd_nop
	.4byte MEScrCmd_checkcompat
	.4byte MEScrCmd_end
	.4byte MEScrCmd_setmsg
	.4byte MEScrCmd_setstatus
	.4byte MEScrCmd_runscript
	.4byte MEScrCmd_initramscript
	.4byte MEScrCmd_setenigmaberry
	.4byte MEScrCmd_giveribbon
	.4byte MEScrCmd_givenationaldex
	.4byte MEScrCmd_addrareword
	.4byte MEScrCmd_setrecordmixinggift
	.4byte MEScrCmd_givepokemon
	.4byte MEScrCmd_addtrainer
	.4byte MEScrCmd_enableresetrtc
	.4byte MEScrCmd_checksum
	.4byte MEScrCmd_crc
#endif
gMysteryEventScriptCmdTableEnd::
