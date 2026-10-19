// Applies symbol files to the program. Each line: "<hex address> <name> [signature]".
// Names may contain "::" (Class::Method); class namespaces are created and
// methods in them become __thiscall. Addresses in executable blocks become
// functions, everything else a label. If a C signature follows the name it is
// parsed and applied (e.g. "00012345 CMenu::Draw void __thiscall CMenu::Draw(int x)").
// Later files override earlier ones.
// Usage (headless postScript): ApplySymbols.java <file> [<file> ...]
//@category Xbox

import java.io.File;
import java.nio.file.Files;

import ghidra.app.cmd.function.ApplyFunctionSignatureCmd;
import ghidra.app.script.GhidraScript;
import ghidra.app.util.parser.FunctionSignatureParser;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.FunctionDefinitionDataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolTable;
import ghidra.app.util.NamespaceUtils;
import ghidra.program.model.listing.GhidraClass;
import ghidra.app.services.DataTypeQueryService;

public class ApplySymbols extends GhidraScript {

	@Override
	protected void run() throws Exception {
		SymbolTable st = currentProgram.getSymbolTable();
		int n = 0, bad = 0;
		for (String path : getScriptArgs()) {
			for (String raw : Files.readAllLines(new File(path).toPath())) {
				String line = raw.trim();
				if (line.isEmpty() || line.startsWith("#")) {
					continue;
				}
				String[] f = line.split("\\s+", 3);
				try {
					apply(st, toAddr(Long.parseLong(f[0], 16)), f[1], f.length > 2 ? f[2] : null);
					n++;
				}
				catch (Exception e) {
					printerr(path + ": " + line + ": " + e.getMessage());
					bad++;
				}
			}
		}
		println("ApplySymbols: applied " + n + ", failed " + bad);
	}

	private void apply(SymbolTable st, Address a, String fullName, String sig) throws Exception {
		Namespace ns = currentProgram.getGlobalNamespace();
		String name = fullName;
		int sep = fullName.lastIndexOf("::");
		if (sep > 0) {
			ns = NamespaceUtils.createNamespaceHierarchy(fullName.substring(0, sep), null, currentProgram, SourceType.USER_DEFINED);
			if (!(ns instanceof GhidraClass)) {
				ns = NamespaceUtils.convertNamespaceToClass(ns);
			}
			name = fullName.substring(sep + 2);
		}

		MemoryBlock b = getMemoryBlock(a);
		if (b == null || !b.isExecute()) {
			Symbol s = st.getPrimarySymbol(a);
			if (s != null && s.getSource() != SourceType.DEFAULT && s.getName(true).equals(fullName)) {
				return;
			}
			createLabel(a, name, ns, true, SourceType.USER_DEFINED);
			return;
		}

		Function fn = getFunctionAt(a);
		if (fn == null) {
			disassemble(a);
			fn = createFunction(a, null);
			if (fn == null) {
				throw new Exception("could not create function");
			}
		}
		fn.setName(name, SourceType.USER_DEFINED);
		fn.setParentNamespace(ns);
		if (sig != null) {
			FunctionSignatureParser p = new FunctionSignatureParser(currentProgram.getDataTypeManager(),
				state.getTool() == null ? null : state.getTool().getService(DataTypeQueryService.class));
			FunctionDefinitionDataType def = p.parse(fn.getSignature(), sig);
			new ApplyFunctionSignatureCmd(a, def, SourceType.USER_DEFINED).applyTo(currentProgram, monitor);
		}
		else if (ns instanceof GhidraClass) {
			fn.setCallingConvention("__thiscall");
		}
	}
}
