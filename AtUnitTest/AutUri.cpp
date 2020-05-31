#include "AutIncludes.h"
#include "AutMain.h"



void FindAndCheckUris(Seq text, ParseTree::Storage* storage, Vec<Seq>& foundUris, Vec<Seq> const& expectUris, Display display)
{
	foundUris.ReserveExact(expectUris.Len());
	foundUris.Clear();
	Uri::FindUrisInText(text, foundUris, storage);

	if (display == Display::Yes)
	{
		for (sizet i=0; i!=foundUris.Len(); ++i)
			Console::Out(Str("Found URI ").UInt(i).Add(": ").Add(foundUris[i]).Add("\r\n"));
	}

	if (foundUris.Len() != expectUris.Len())
		throw StrErr(Str("Number of found URIs ").UInt(foundUris.Len()).Add(" does not match the number of expected URIs ").UInt(expectUris.Len()));

	for (sizet i=0; i!=foundUris.Len(); ++i)
		if (!foundUris[i].EqualExact(expectUris[i]))
			throw StrErr(Str::Join("Found URI '", foundUris[i], "' does not match expected URI '", expectUris[i], "'"));

	if (display == Display::Yes)
		Console::Out("OK: all found URIs match expected URIs\r\n");
}


void UriTests()
{
	char const* const c_zText =
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit: https://example.com sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
		"Ut enim ad minim [veniam](https://example.com/path#fragment), quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. "
		"Duis aute irure dolor in <a href='https://www.example.com/Path%20with%20escapes'>reprehenderit</a> in voluptate velit esse cillum dolore eu fugiat nulla pariatur. "
		"Excepteur ssh://user@host.example.com sint ssh://user@host.example.com:2222 occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.\r\n"
		"\r\n"
		"ssh://user;fingerprint=ssh-dss-c1-b1-30-29-d7-b8-de-6c-97-77-10-d7-46-41-63-87@host.example.com\r\n"
		"https://example.com\r\n"
		"https://example.com/\r\n"
		"'https://example.com/Path%20with%20escapes\r\n"
		"(https://example.com/Path%20with%20escapes/and#fragment\r\n"
		"\r\n"
		"Curabitur pretium tincidunt lacus. Nulla gravida orci a odio. Nullam varius, turpis et commodo pharetra, est eros bibendum elit, "
		"nec luctus magna felis sollicitudin mauris. 'https://www.example.com/path?with%20param' Integer in mauris eu nibh euismod gravida. Duis ac tellus et risus vulputate vehicula. "
		"Donec lobortis risus a elit. Etiam tempor. mailto:email+add.ress@example.com Ut ullamcorper, ligula eu tempor congue, eros est euismod turpis, id tincidunt sapien risus a quam. "
		"Maecenas fermentum consequat mi. <a href='mailto:email+add.ress2@example.com'>Donec</a> fermentum. Pellentesque malesuada nulla a mi. Duis sapien sem, aliquet nec, commodo eget, "
		"consequat quis, neque. Aliquam faucibus, elit ut dictum aliquet, felis nisl adipiscing sapien, sed malesuada diam lacus eget erat. Cras mollis scelerisque nunc. Nullam arcu. "
		"Aliquam consequat. [Curabitur](mailto:email(address3)@example.com) augue lorem, dapibus quis, laoreet et, pretium ac, nisi. Aenean magna nisl, mollis quis, molestie eu, feugiat in, orci. "
		"In hac habitasse platea dictumst.\r\n";

	Seq const c_text = c_zText;

	enum { NrExpectUris = 14 };
	Vec<Seq> expectUris;
	expectUris.ReserveExact(NrExpectUris);
	expectUris.Add("https://example.com");
	expectUris.Add("https://example.com/path#fragment");
	expectUris.Add("https://www.example.com/Path%20with%20escapes");
	expectUris.Add("ssh://user@host.example.com");
	expectUris.Add("ssh://user@host.example.com:2222");
	expectUris.Add("ssh://user;fingerprint=ssh-dss-c1-b1-30-29-d7-b8-de-6c-97-77-10-d7-46-41-63-87@host.example.com");
	expectUris.Add("https://example.com");
	expectUris.Add("https://example.com/");
	expectUris.Add("https://example.com/Path%20with%20escapes");
	expectUris.Add("https://example.com/Path%20with%20escapes/and#fragment");
	expectUris.Add("https://www.example.com/path?with%20param");
	expectUris.Add("mailto:email+add.ress@example.com");
	expectUris.Add("mailto:email+add.ress2@example.com");
	expectUris.Add("mailto:email(address3)@example.com");
	EnsureThrow(expectUris.Len() == NrExpectUris);

	ParseTree::Storage storage;
	Vec<Seq> foundUris;
	FindAndCheckUris(c_text, &storage, foundUris, expectUris, Display::Yes);
}
