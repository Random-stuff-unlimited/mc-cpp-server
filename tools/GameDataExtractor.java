import java.io.BufferedReader;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;
import java.util.TreeMap;

/**
 * Reads values that only exist in the game's code (not in the data generator reports) by loading the official
 * server jar and calling its methods through reflection. Run by tools/update_gamedata.py:
 *
 *   java -cp <server jar + libraries> tools/GameDataExtractor.java <server_mappings.txt | -> <output.json>
 *
 * The jar is obfuscated: names are translated with Mojang's official mappings ("-" when the jar isn't obfuscated).
 * Output: {"blocks": {block: {property: value}}, "states": {property: [value per state id]}, "block_items": {item: block}}
 */
public class GameDataExtractor {
	// Deobfuscated class -> obfuscated class
	private static final Map<String, String> classes = new HashMap<>();
	// "class#name(params)" or "class#field" -> obfuscated member name
	private static final Map<String, String> members = new HashMap<>();
	private static boolean obfuscated;

	public static void main(String[] args) throws Exception {
		obfuscated = !args[0].equals("-");
		if (obfuscated) readMappings(args[0]);

		invokeStatic("net.minecraft.SharedConstants", "tryDetectVersion");
		invokeStatic("net.minecraft.server.Bootstrap", "bootStrap");

		Object	 blocks	   = staticField("net.minecraft.core.registries.BuiltInRegistries", "BLOCK");
		Object	 items	   = staticField("net.minecraft.core.registries.BuiltInRegistries", "ITEM");
		Method	 getKey	   = method("net.minecraft.core.Registry", "getKey", "java.lang.Object");
		Class<?> blockItem = find("net.minecraft.world.item.BlockItem");
		Method	 getBlock  = method("net.minecraft.world.item.BlockItem", "getBlock", "");

		// Per block
		String	 block			  = "net.minecraft.world.level.block.Block";
		String[] blockProperties  = {"destroy_time", "explosion_resistance", "friction", "speed_factor", "jump_factor"};
		Method[] blockGetters	  = {method("net.minecraft.world.level.block.state.BlockBehaviour", "defaultDestroyTime", ""),
									 method(block, "getExplosionResistance", ""), method(block, "getFriction", ""),
									 method(block, "getSpeedFactor", ""), method(block, "getJumpFactor", "")};
		// Per block state, indexed by state id
		String	 stateBase		  = "net.minecraft.world.level.block.state.BlockBehaviour$BlockStateBase";
		String[] stateProperties  = {"light_emission", "requires_correct_tool", "occludes", "light_block", "propagates_skylight_down"};
		Method[] stateGetters	  = {method(stateBase, "getLightEmission", ""), method(stateBase, "requiresCorrectToolForDrops", ""),
									 method(stateBase, "canOcclude", ""), method(stateBase, "getLightBlock", ""),
									 method(stateBase, "propagatesSkylightDown", "")};
		Method	 stateDefinition  = method(block, "getStateDefinition", "");
		Method	 possibleStates	  = method("net.minecraft.world.level.block.state.StateDefinition", "getPossibleStates", "");
		Method	 stateId		  = method(block, "getId", "net.minecraft.world.level.block.state.BlockState");

		StringBuilder	   blocksJson  = new StringBuilder();
		Map<Integer, Object[]> states = new TreeMap<>();
		for (Object b : (Iterable<?>) blocks) {
			if (blocksJson.length() > 0) blocksJson.append(",");
			blocksJson.append("\"").append(getKey.invoke(blocks, b)).append("\":{");
			for (int i = 0; i < blockProperties.length; i++) {
				blocksJson.append(i > 0 ? "," : "").append("\"").append(blockProperties[i]).append("\":").append(blockGetters[i].invoke(b));
			}
			blocksJson.append("}");
			for (Object state : (Iterable<?>) possibleStates.invoke(stateDefinition.invoke(b))) {
				Object[] values = new Object[stateGetters.length];
				for (int i = 0; i < stateGetters.length; i++) values[i] = stateGetters[i].invoke(state);
				states.put((Integer) stateId.invoke(null, state), values);
			}
		}

		StringBuilder statesJson = new StringBuilder();
		for (int i = 0; i < stateProperties.length; i++) {
			statesJson.append(i > 0 ? "," : "").append("\"").append(stateProperties[i]).append("\":[");
			int n = 0;
			for (Object[] values : states.values()) statesJson.append(n++ > 0 ? "," : "").append(values[i]);
			statesJson.append("]");
		}

		Map<String, String> blockItems = new TreeMap<>();
		for (Object item : (Iterable<?>) items) {
			if (!blockItem.isInstance(item)) continue;
			blockItems.put(getKey.invoke(items, item).toString(), getKey.invoke(blocks, getBlock.invoke(item)).toString());
		}

		try (FileWriter out = new FileWriter(args[1])) {
			out.write("{\"blocks\":{" + blocksJson + "},\"states\":{" + statesJson + "},\"block_items\":{");
			writeEntries(out, blockItems);
			out.write("}}\n");
		}
		System.out.println("Extracted " + states.size() + " block states and " + blockItems.size() + " block items");
		System.exit(0); // The game leaves non-daemon threads running
	}

	private static void writeEntries(FileWriter out, Map<String, ?> entries) throws IOException {
		boolean first = true;
		for (Map.Entry<String, ?> e : entries.entrySet()) {
			if (!first) out.write(",");
			first = false;
			Object value = e.getValue();
			out.write("\"" + e.getKey() + "\":" + (value instanceof String ? "\"" + value + "\"" : value.toString()));
		}
	}

	// ===== Mappings =====

	private static void readMappings(String path) throws IOException {
		try (BufferedReader in = new BufferedReader(new FileReader(path))) {
			String line;
			String current = null;
			while ((line = in.readLine()) != null) {
				if (line.startsWith("#") || line.isBlank()) continue;
				if (!line.startsWith(" ")) {
					// "net.minecraft.Foo -> abc:"
					String[] parts = line.split(" -> ");
					current		   = parts[0];
					classes.put(current, parts[1].substring(0, parts[1].length() - 1));
					continue;
				}
				// "    12:34:void name(int,java.lang.String) -> a" or "    int field -> b"
				String[] parts	   = line.trim().split(" -> ");
				String	 signature = parts[0].replaceFirst("^\\d+:\\d+:", "");
				String	 name	   = signature.substring(signature.indexOf(' ') + 1);
				members.put(current + "#" + name, parts[1]);
			}
		}
	}

	private static Class<?> find(String name) throws ClassNotFoundException {
		String obf = obfuscated ? classes.get(name) : name;
		if (obf == null) throw new IllegalStateException("No mapping for class " + name);
		return Class.forName(obf);
	}

	private static String member(String owner, String key, String name) {
		if (!obfuscated) return name;
		String obf = members.get(owner + "#" + key);
		if (obf == null) throw new IllegalStateException("No mapping for " + owner + "#" + key);
		return obf;
	}

	// Parameters as written in the mappings, e.g. "java.lang.Object" or "". Obfuscation gives overloads the same
	// name, so the parameter types have to match too
	private static Method method(String owner, String name, String params) throws Exception {
		String	 obfName = member(owner, name + "(" + params + ")", name);
		String[] wanted	 = params.isEmpty() ? new String[0] : params.split(",");
		for (int i = 0; i < wanted.length; i++) {
			String base	  = wanted[i].replace("[]", "");
			String mapped = obfuscated && classes.containsKey(base) ? classes.get(base) : base;
			wanted[i]	  = mapped + wanted[i].substring(base.length());
		}
		for (Class<?> c = find(owner); c != null; c = c.getSuperclass()) {
			for (Method m : c.getDeclaredMethods()) {
				if (!m.getName().equals(obfName) || m.getParameterCount() != wanted.length) continue;
				boolean match = true;
				for (int i = 0; i < wanted.length; i++) match &= m.getParameterTypes()[i].getTypeName().equals(wanted[i]);
				if (!match) continue;
				m.setAccessible(true);
				return m;
			}
		}
		throw new NoSuchMethodException(owner + "." + name + "(" + params + ")");
	}

	private static void invokeStatic(String owner, String name) throws Exception { method(owner, name, "").invoke(null); }

	private static Object staticField(String owner, String name) throws Exception {
		Field field = find(owner).getDeclaredField(member(owner, name, name));
		field.setAccessible(true);
		return field.get(null);
	}
}
