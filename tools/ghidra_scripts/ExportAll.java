// Dumps analysis results for grepping outside Ghidra:
//   <out>/functions.tsv   address, size, block, name, callers, callees
//   <out>/strings.tsv     address, referencing functions, string
//   <out>/decomp/<block>.c raw decompiler output for every function
// Usage (headless postScript): ExportAll.java <outdir>
//@category Xbox

import java.io.File;
import java.io.PrintWriter;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.TreeSet;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;

public class ExportAll extends GhidraScript {

	@Override
	protected void run() throws Exception {
		File out = new File(getScriptArgs()[0]);
		File decompDir = new File(out, "decomp");
		decompDir.mkdirs();

		try (PrintWriter w = new PrintWriter(new File(out, "functions.tsv"))) {
			w.println("address\tsize\tblock\tname\tcallers\tcallees");
			for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
				MemoryBlock b = getMemoryBlock(f.getEntryPoint());
				w.printf("%s\t%d\t%s\t%s\t%d\t%d%n", f.getEntryPoint(), f.getBody().getNumAddresses(),
					b == null ? "?" : b.getName(), f.getName(true),
					f.getCallingFunctions(monitor).size(), f.getCalledFunctions(monitor).size());
			}
		}

		try (PrintWriter w = new PrintWriter(new File(out, "strings.tsv"))) {
			for (Data d : currentProgram.getListing().getDefinedData(true)) {
				if (!d.hasStringValue()) {
					continue;
				}
				Set<String> users = new TreeSet<>();
				for (Reference r : getReferencesTo(d.getAddress())) {
					Function f = getFunctionContaining(r.getFromAddress());
					users.add(f == null ? r.getFromAddress().toString() : f.getName(true));
				}
				String v = String.valueOf(d.getValue()).replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t");
				w.printf("%s\t%s\t%s%n", d.getAddress(), String.join(",", users), v);
			}
		}

		DecompInterface di = new DecompInterface();
		di.openProgram(currentProgram);
		Map<String, PrintWriter> files = new HashMap<>();
		try {
			for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
				if (monitor.isCancelled()) {
					break;
				}
				MemoryBlock b = getMemoryBlock(f.getEntryPoint());
				String block = b == null ? "unknown" : b.getName().replaceAll("[^A-Za-z0-9_]", "");
				PrintWriter w = files.get(block);
				if (w == null) {
					w = new PrintWriter(new File(decompDir, block + ".c"));
					files.put(block, w);
				}
				DecompileResults r = di.decompileFunction(f, 60, monitor);
				w.printf("// ==== %s @ %s%n", f.getName(true), f.getEntryPoint());
				w.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// decompile failed: " + r.getErrorMessage());
			}
		}
		finally {
			for (PrintWriter w : files.values()) {
				w.close();
			}
			di.dispose();
		}
	}
}
