<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
    <!--
        In the man page, one can not distinguish fixed font text from ordinary
        text. However, we use fixed font in examples to indicate that the text
        can be copied verbatim. In order to make this visible in the man page,
        we replace fixed font by strong in the intermediate xml format. (I don't
        want to use the same format in the source txt file, because in the web
        version, one can perfectly distinguish bold and fixed font)
    -->
    <!-- the present xsl sheet is a modification of
         https://stackoverflow.com/a/6113231/4400896
    -->
    <xsl:output method="xml" encoding="UTF-8" omit-xml-declaration="no" indent="yes"/>
    <!-- Do not strip spaces, preserve them per default
    https://www.data2type.de/xml-xslt-xslfo/xslt/xslt-referenz/preserve-space
    <xsl:strip-space elements="*"/>
    -->
    <xsl:preserve-space elements="*"/>


    <!-- essentially, the following rules immitate the sed expression:
     's,<literal>,<emphasis role=\"strong\">,g\;s,</literal>,</emphasis>,g'
    -->

    <xsl:template match="node()|@*">
        <xsl:copy>
            <xsl:apply-templates select="node()|@*"/>
        </xsl:copy>
    </xsl:template>

    <xsl:template match="literal">
        <emphasis role="strong"><xsl:value-of select="."/></emphasis>
    </xsl:template>

</xsl:stylesheet>
