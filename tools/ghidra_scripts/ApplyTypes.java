// Parses a C header (symbols/types.h) into the program's data type manager.
// Usage (headless postScript): ApplyTypes.java <types.h>
//@category Xbox

import java.io.File;
import java.nio.file.Files;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.cparser.C.CParser;
import ghidra.program.model.data.DataTypeManager;

public class ApplyTypes extends GhidraScript {

	@Override
	protected void run() throws Exception {
		String src = Files.readString(new File(getScriptArgs()[0]).toPath());
		DataTypeManager dtm = currentProgram.getDataTypeManager();
		CParser p = new CParser(dtm, true, null);
		p.parse(src);
		println("ApplyTypes: parsed " + p.getComposites().size() + " composites, " + p.getTypes().size() + " typedefs");
	}
}
