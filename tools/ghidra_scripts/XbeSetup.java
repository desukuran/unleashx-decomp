// Turns a flat XBE memory image (see tools/xbe.py flat) into a proper program:
// splits/names/protects the XBE sections, marks the entry point and binds the
// kernel thunk table to xboxkrnl.exe externals with stdcall/fastcall signatures.
// Usage (headless preScript): XbeSetup.java <layout.txt>
//@category Xbox

import java.io.File;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DWordDataType;
import ghidra.program.model.data.PointerDataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.listing.ParameterImpl;
import ghidra.program.model.listing.ReturnParameterImpl;
import ghidra.program.model.listing.Parameter;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.ExternalLocation;
import ghidra.program.model.symbol.ExternalManager;
import ghidra.program.model.symbol.RefType;
import ghidra.program.model.symbol.SourceType;

public class XbeSetup extends GhidraScript {

	private static final int SEC_WRITABLE = 0x1;
	private static final int SEC_EXECUTABLE = 0x4;
	private static final Set<String> DATA_SECTIONS = Set.of(".rdata", ".data", "DOLBY", "$$XTIMAGE",
		"$$XSIMAGE", "MEDIA_FILE", "DEF_FONT", "INT_SKINS", ".XTLID");

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		List<String> lines = Files.readAllLines(new File(args[0]).toPath());
		Memory mem = currentProgram.getMemory();

		List<String[]> sections = new ArrayList<>();
		List<String[]> kernel = new ArrayList<>();
		long base = 0, headersSize = 0, entry = 0;
		for (String line : lines) {
			String[] f = line.trim().split("\\s+");
			switch (f[0]) {
				case "base": base = Long.decode(f[1]); headersSize = Long.decode(f[2]); break;
				case "entry": entry = Long.decode(f[1]); break;
				case "section": sections.add(f); break;
				case "kernel": kernel.add(f); break;
			}
		}

		// Split the single flat block at every section start.
		for (String[] s : sections) {
			Address a = toAddr(Long.decode(s[2]));
			MemoryBlock b = mem.getBlock(a);
			if (!b.getStart().equals(a)) {
				mem.split(b, a);
			}
		}
		MemoryBlock hdr = mem.getBlock(toAddr(base));
		hdr.setName("XBE_HEADER");
		hdr.setPermissions(true, false, false);
		for (String[] s : sections) {
			MemoryBlock b = mem.getBlock(toAddr(Long.decode(s[2])));
			int flags = Integer.decode(s[4]);
			b.setName(s[1]);
			// XBE section flags mark nearly everything executable; only trust it
			// for real code sections so data isn't disassembled.
			boolean code = (flags & SEC_EXECUTABLE) != 0 && !DATA_SECTIONS.contains(s[1]);
			b.setPermissions(true, (flags & SEC_WRITABLE) != 0, code);
		}

		// Kernel imports.
		ExternalManager em = currentProgram.getExternalManager();
		FunctionManager fm = currentProgram.getFunctionManager();
		for (String[] k : kernel) {
			Address a = toAddr(Long.decode(k[1]));
			String name = k[3], conv = k[4];
			int argBytes = Integer.parseInt(k[5]);
			createLabel(a, "__imp_" + name, true, SourceType.IMPORTED);
			clearListing(a, a.add(3));
			if (conv.equals("data")) {
				ExternalLocation loc = em.addExtLocation("xboxkrnl.exe", name, null, SourceType.IMPORTED);
				createData(a, PointerDataType.dataType);
				currentProgram.getReferenceManager().addExternalReference(a, 0, loc, SourceType.IMPORTED, RefType.DATA);
				continue;
			}
			ExternalLocation loc = em.addExtFunction("xboxkrnl.exe", name, null, SourceType.IMPORTED);
			createData(a, PointerDataType.dataType);
			currentProgram.getReferenceManager().addExternalReference(a, 0, loc, SourceType.IMPORTED, RefType.DATA);
			Function fn = loc.getFunction();
			if (fn != null) {
				List<Parameter> params = new ArrayList<>();
				for (int i = 0; i < argBytes / 4; i++) {
					params.add(new ParameterImpl("param_" + (i + 1), DWordDataType.dataType, currentProgram));
				}
				fn.updateFunction(conv.equals("fastcall") ? "__fastcall" : conv.equals("cdecl") ? "__cdecl" : "__stdcall",
					new ReturnParameterImpl(DWordDataType.dataType, currentProgram), params,
					Function.FunctionUpdateType.DYNAMIC_STORAGE_ALL_PARAMS, true, SourceType.IMPORTED);
			}
		}

		Address ep = toAddr(entry);
		addEntryPoint(ep);
		createFunction(ep, "entry");
		disassemble(ep);
		println("XbeSetup: " + sections.size() + " sections, " + kernel.size() + " kernel imports, entry " + ep);
	}
}
