// Args: outFile singletonRva disp...  Functions that call the singleton getter (or use its inlined global)
// and contain an instruction with the given displacement (e.g. FixedStrings +0x5c0 = bForceIdleStop).
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.Reference;
import java.io.*;
import java.util.*;
public class FieldUse extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        long base = currentProgram.getImageBase().getOffset();
        Set<Function> fns = new LinkedHashSet<>();
        for (String s : a[1].split(",")) {
            for (Reference r : getReferencesTo(toAddr(base + Long.parseLong(s, 16)))) {
                Function f = getFunctionContaining(r.getFromAddress());
                if (f != null) fns.add(f);
            }
        }
        try (PrintWriter w = new PrintWriter(new FileWriter(a[0]))) {
            w.println("callers: " + fns.size());
            for (int i = 2; i < a.length; ++i) {
                String disp = "0x" + a[i].toLowerCase();
                w.println("== disp " + disp);
                for (Function f : fns) {
                    InstructionIterator it = currentProgram.getListing().getInstructions(f.getBody(), true);
                    while (it.hasNext()) {
                        Instruction ins = it.next();
                        String t = ins.toString().toLowerCase();
                        if (t.contains("+ " + disp + "]") || t.contains("+" + disp + "]")) {
                            w.println("  " + f.getName() + " @+" + Long.toHexString(f.getEntryPoint().getOffset() - base) + "  +" +
                                Long.toHexString(ins.getAddress().getOffset() - base) + "  " + ins);
                        }
                    }
                }
            }
        }
    }
}
