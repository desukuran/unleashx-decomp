// Applies symbol files to the program. Each line: "<hex address> <name> [signature]".
// Names may contain "::" (Class::Method); class namespaces are created and
// methods in them become __thiscall. Addresses in executable blocks become
// functions, everything else a label. If a C signature follows the name it is
// parsed and applied (e.g. "00012345 CMenu::Draw void __thiscall CMenu::Draw(int x)").
// A parameter may be pinned to a register with "@reg" after its name (e.g.
// @eax, or @xmm0 for a float in an SSE register), for the
// custom conventions MSVC's link-time code generation produces, e.g.
// "000117e4 std::string::ctor_cstr std_string * f(std_string *self@eax, char *s)".
// Remaining parameters are laid out on the stack in order.
// A signature of just "data" makes the symbol a label even in a code section.
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
import ghidra.program.model.data.DataType;
import ghidra.program.model.lang.Register;
import ghidra.program.model.listing.Parameter;
import ghidra.program.model.listing.ParameterImpl;
import ghidra.program.model.listing.ReturnParameterImpl;
import ghidra.program.model.listing.VariableStorage;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

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

	private static final Pattern CONV = Pattern.compile("\\b(__stdcall|__cdecl|__thiscall|__fastcall)\\b");
	private static final Pattern REG = Pattern.compile("@(e[abcd]x|e[sd]i|ebp|[abcd][lh]|xmm[0-7])\\s*$");

	/** Strips "@reg" markers from the parameter list, recording one entry (or null) per parameter. */
	private static String extractRegisters(String sig, List<String> regs) {
		int open = sig.indexOf('('), close = sig.lastIndexOf(')');
		String[] params = sig.substring(open + 1, close).split(",");
		StringBuilder out = new StringBuilder(sig.substring(0, open + 1));
		for (int i = 0; i < params.length; i++) {
			Matcher m = REG.matcher(params[i]);
			if (m.find()) {
				regs.add(m.group(1));
				out.append(params[i], 0, m.start());
			}
			else {
				regs.add(null);
				out.append(params[i]);
			}
			if (i < params.length - 1) {
				out.append(',');
			}
		}
		return out.append(sig.substring(close)).toString();
	}

	private void applyRegisterStorage(Function fn, List<String> regs) throws Exception {
		Parameter[] old = fn.getParameters();
		List<Parameter> params = new ArrayList<>();
		int stack = 4; // past the return address
		for (int i = 0; i < old.length; i++) {
			DataType dt = old[i].getDataType();
			VariableStorage vs;
			String r = i < regs.size() ? regs.get(i) : null;
			if (r != null) {
				// xmmN holds a float in its low dword (LTCG passes some floats there)
				String name = r.startsWith("xmm") ? r.toUpperCase() + "_Da" : r.toUpperCase();
				Register reg = currentProgram.getRegister(name);
				vs = new VariableStorage(currentProgram, reg);
			}
			else {
				vs = new VariableStorage(currentProgram, stack, dt.getLength());
				stack += (dt.getLength() + 3) & ~3;
			}
			params.add(new ParameterImpl(old[i].getName(), dt, vs, currentProgram));
		}
		DataType rt = fn.getReturnType();
		String retReg = rt instanceof ghidra.program.model.data.AbstractFloatDataType ? "ST0" : "EAX";
		ReturnParameterImpl ret = rt.getLength() <= 0
				? new ReturnParameterImpl(rt, VariableStorage.VOID_STORAGE, currentProgram)
				: new ReturnParameterImpl(rt, new VariableStorage(currentProgram, currentProgram.getRegister(retReg)), currentProgram);
		fn.updateFunction(null, ret, params, Function.FunctionUpdateType.CUSTOM_STORAGE, true, SourceType.USER_DEFINED);
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
		boolean isData = "data".equals(sig);
		if (b == null || !b.isExecute() || isData) {
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
			List<String> regs = new ArrayList<>();
			sig = extractRegisters(sig, regs);
			String cc = null;
			Matcher ccm = CONV.matcher(sig);
			if (ccm.find()) {
				cc = ccm.group(1);
				sig = sig.substring(0, ccm.start()) + sig.substring(ccm.end());
			}
			FunctionSignatureParser p = new FunctionSignatureParser(currentProgram.getDataTypeManager(),
				state.getTool() == null ? null : state.getTool().getService(DataTypeQueryService.class));
			FunctionDefinitionDataType def = p.parse(fn.getSignature(), sig);
			if (cc != null) {
				def.setCallingConvention(cc);
			}
			else if (regs.stream().anyMatch(r -> r != null)) {
				// Custom storage; avoid __thiscall's hidden 'this' parameter.
				def.setCallingConvention("__stdcall");
			}
			else if (ns instanceof GhidraClass && regs.stream().allMatch(r -> r == null)) {
				def.setCallingConvention("__thiscall");
			}
			if (!new ApplyFunctionSignatureCmd(a, def, SourceType.USER_DEFINED).applyTo(currentProgram, monitor)) {
				throw new Exception("signature not applied");
			}
			fn.setName(name, SourceType.USER_DEFINED);
			fn.setParentNamespace(ns);
			if (regs.stream().anyMatch(r -> r != null)) {
				applyRegisterStorage(fn, regs);
			}
		}
		else if (ns instanceof GhidraClass) {
			fn.setCallingConvention("__thiscall");
		}
	}
}
