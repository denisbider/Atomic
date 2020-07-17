#pragma once

enum class Display { No, Yes };

void CoreTests          ();

void ActvTests          ();
void BaseXYTests        ();
void BCryptTests        ();
void BootTime           ();
void DiffTests          (Slice<Seq> args);
void DkimTest           (Slice<Seq> args);
void EmailAddressTest   (Slice<Seq> args);
void EntityStoreTests   (Slice<Seq> args);
void HtmlEmbedTest      (Args& args);
void LargeEntitiesTests ();
void MapTests           ();
void MarkdownTests      (Slice<Seq> args);
void MpUIntTests        ();
void MultipartTests     ();
void RsaSignerTests     ();
void SchannelClientTest (Slice<Seq> args);
void SmtpReceiverTest   ();
void TextBuilderTests   ();
void UriTests           ();
void WinErrTest         (Slice<Seq> args);
