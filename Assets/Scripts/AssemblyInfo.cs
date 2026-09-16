using System.Runtime.CompilerServices;

// Allows the EditMode test assembly to see internal battle-format members
// (e.g. AstralReserveCollection.TryAdd) now that runtime code and tests
// live in separate assemblies (AstralWilds.Runtime / AstralWilds.Tests.EditMode).
[assembly: InternalsVisibleTo("AstralWilds.Tests.EditMode")]
